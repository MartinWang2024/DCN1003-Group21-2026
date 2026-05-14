#include "server_main.h"

#include "cmdreg.h"
#include "listener.h"
#include "log.h"
#include "protocol.h"
#include "winsock_guard.h"

#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <utility>

using namespace Protocal::Dispatch;

void handle_client(TcpSocket::SocketHandler sh,
                   dcn_database::CourseRepository& courses,
                   dcn_database::AdministratorRepository& admins,
                   Dispatcher& dispatcher)
{
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
    // 首次启动注入默认管理员, 否则任何客户端都登不上
    if (!admins.verify_login("admin", "admin123"))
    {
        admins.insert_or_replace({"admin", "admin123"});
        print_log(info, "Default admin account injected: admin/admin123");
    }

    Dispatcher dispatcher;
    register_all_server(dispatcher);

    SOCKET listen_sock = create_listener(PORT, BACKLOG);
    if (listen_sock == INVALID_SOCKET) return 1;

    while (true)
    {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock,
                                    reinterpret_cast<sockaddr*>(&client_addr),
                                    &addr_len);
        if (client_sock == INVALID_SOCKET)
        {
            std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        print_log(info, "[+] Client connected: %s:%d", ip, ntohs(client_addr.sin_port));

        TcpSocket::SocketHandler sh(client_sock, client_addr);
        std::thread(handle_client,
                    std::move(sh),
                    std::ref(courses), std::ref(admins),
                    std::ref(dispatcher)).detach();
    }

    closesocket(listen_sock);
    return 0;
}
