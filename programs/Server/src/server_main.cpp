#include "server_main.h"

#include "cmdreg.h"
#include "listener.h"
#include "log.h"
#include "protocol.h"
#include "winsock_guard.h"

#include <ws2tcpip.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>

using namespace Protocal::Dispatch;

namespace {
std::atomic<SOCKET> g_listen_sock{INVALID_SOCKET};
std::atomic<bool>   g_running{true};

std::mutex                g_clients_mu;
std::unordered_set<SOCKET> g_client_socks;
std::atomic<int>          g_active_workers{0};

void track_client(SOCKET s) {
    std::lock_guard<std::mutex> lk(g_clients_mu);
    g_client_socks.insert(s);
}

void untrack_client(SOCKET s) {
    std::lock_guard<std::mutex> lk(g_clients_mu);
    g_client_socks.erase(s);
}

void shutdown_all_clients() {
    std::lock_guard<std::mutex> lk(g_clients_mu);
    for (SOCKET s : g_client_socks) {
        // SD_BOTH unblocks any pending recv on the worker thread without
        // closing the handle; the worker's RAII SocketHandler still owns it.
        shutdown(s, SD_BOTH);
    }
}

void request_shutdown() {
    g_running = false;
    SOCKET s = g_listen_sock.exchange(INVALID_SOCKET);
    if (s != INVALID_SOCKET) {
        closesocket(s);
    }
    shutdown_all_clients();
}

BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            print_log(info, "[*] shutdown signal received, draining...");
            request_shutdown();
            return TRUE;
        default:
            return FALSE;
    }
}

void signal_handler(int) { request_shutdown(); }
}

void handle_client(TcpSocket::SocketHandler sh,
                   SOCKET raw_sock,
                   dcn_database::CourseRepository& courses,
                   dcn_database::AdministratorRepository& admins,
                   Dispatcher& dispatcher)
{
    g_active_workers.fetch_add(1);
    Session_t session;

    while (true)
    {
        MsgBody body;
        auto rerr = Protocal::Package_receive(sh, body);
        if (rerr.e != Error::SUCCESS)
        {
            print_log(info, "handle_client: recv ended (%s)", rerr.message.c_str());
            break;
        }

        ReqContext_t ctx{body, session, courses, admins};
        Response_t resp = dispatcher.dispatch(ctx);

        auto serr = Dispatcher::send_response_s(sh, resp);
        if (serr.e != Error::SUCCESS)
        {
            print_log(warn, "handle_client: send failed (%s)", serr.message.c_str());
            break;
        }
    }

    print_log(info, "handle_client: session for '%s' closed",
              session.name.empty() ? "<anon>" : session.name.c_str());
    untrack_client(raw_sock);
    g_active_workers.fetch_sub(1);
}


int main()
{
    TcpSocket::WinsockGuard winsock;
    if (!winsock.ok())
    {
        std::cerr << winsock.last_error().message << std::endl;
        return 1;
    }

    if (openssl::readAppKey(openssl::key, "app.key").e != Error::SUCCESS)
    {
        std::cerr << "Failed to load app.key from working directory" << std::endl;
        return 1;
    }

    dcn_database::CourseRepository courses;
    dcn_database::AdministratorRepository admins;
    if (!courses.open(DB_COURSES_PATH) || !courses.initialize_schema())
    {
        std::cerr << "Failed to open/init courses DB: " << courses.last_error() << std::endl;
        return 1;
    }
    if (!admins.open(DB_ADMINS_PATH) || !admins.initialize_schema())
    {
        std::cerr << "Failed to open/init admins DB: " << admins.last_error() << std::endl;
        return 1;
    }
    if (!admins.verify_login("admin", "admin123"))
    {
        admins.insert_or_replace({"admin", "admin123"});
        print_log(info, "Default admin account injected: admin/admin123");
    }

    Dispatcher dispatcher;
    register_all_server(dispatcher);

    SOCKET listen_sock = create_listener(PORT, BACKLOG);
    if (listen_sock == INVALID_SOCKET) return 1;
    g_listen_sock = listen_sock;

    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    while (g_running)
    {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock,
                                    reinterpret_cast<sockaddr*>(&client_addr),
                                    &addr_len);
        if (client_sock == INVALID_SOCKET)
        {
            if (!g_running) break;
            std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        print_log(info, "[+] Client connected: %s:%d", ip, ntohs(client_addr.sin_port));

        track_client(client_sock);
        TcpSocket::SocketHandler sh(client_sock, client_addr);
        std::thread(handle_client,
                    std::move(sh),
                    client_sock,
                    std::ref(courses), std::ref(admins),
                    std::ref(dispatcher)).detach();
    }

    print_log(info, "[*] server shutting down, draining workers...");
    shutdown_all_clients();

    // Wait for detached workers to release sqlite handles before close().
    // 5s ceiling: workers should exit immediately after shutdown() unblocks recv.
    using namespace std::chrono;
    const auto deadline = steady_clock::now() + seconds(5);
    while (g_active_workers.load() > 0 && steady_clock::now() < deadline) {
        std::this_thread::sleep_for(milliseconds(20));
    }
    if (g_active_workers.load() > 0) {
        print_log(warn, "[!] %d worker(s) did not exit in time; closing DB anyway",
                  g_active_workers.load());
    }

    print_log(info, "[*] flushing DB...");
    courses.close();
    admins.close();
    print_log(info, "[*] server stopped cleanly");
    return 0;
}
