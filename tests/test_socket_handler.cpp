// Tests SocketHandler RAII, move semantics, and send/recv loop correctness.
// Uses a real loopback (127.0.0.1) TCP socket pair for behavioral verification.
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "SocketHandler.h"
#include "test.h"

#pragma comment(lib, "Ws2_32.lib")

using TcpSocket::SocketHandler;

// ─────────────────────────────────────────────
// One-shot Winsock init / teardown.
// ─────────────────────────────────────────────
struct WinsockGuard {
    WinsockGuard() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            throw std::runtime_error("WSAStartup failed");
    }
    ~WinsockGuard() { WSACleanup(); }
};

// ─────────────────────────────────────────────
// Build a loopback socket pair and return both ends as SocketHandlers.
// Same helper as test_protocol; duplicated here to avoid cross-file dependencies.
// ─────────────────────────────────────────────
struct SockPair {
    SocketHandler server;
    SocketHandler client;
};

static SockPair make_loopback_pair() {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) throw std::runtime_error("listener socket failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        throw std::runtime_error("bind failed");
    if (listen(listener, 1) == SOCKET_ERROR)
        throw std::runtime_error("listen failed");

    int alen = sizeof(addr);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &alen) == SOCKET_ERROR)
        throw std::runtime_error("getsockname failed");

    SOCKET cli = socket(AF_INET, SOCK_STREAM, 0);
    if (cli == INVALID_SOCKET) throw std::runtime_error("client socket failed");

    std::thread connector([cli, &addr] {
        connect(cli, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    });

    sockaddr_in peer{};
    int plen = sizeof(peer);
    SOCKET srv = accept(listener, reinterpret_cast<sockaddr*>(&peer), &plen);
    connector.join();

    if (srv == INVALID_SOCKET) throw std::runtime_error("accept failed");
    closesocket(listener);

    sockaddr_in cli_addr{};
    int clen = sizeof(cli_addr);
    getsockname(cli, reinterpret_cast<sockaddr*>(&cli_addr), &clen);

    return SockPair{
        SocketHandler(srv, peer),
        SocketHandler(cli, cli_addr),
    };
}


// ========== Basic send / recv ==========

TEST(test_basic_send_recv)
{
    auto p = make_loopback_pair();
    const char msg[] = "hello world";
    REQUIRE(p.client.socket_send(msg, sizeof(msg)).e == Error::SUCCESS);

    char buf[sizeof(msg)] = {};
    REQUIRE(p.server.socket_recv(buf, sizeof(buf)).e == Error::SUCCESS);
    REQUIRE(std::string(buf) == "hello world");
}

// Verifies the data_ptr+total_recv bug at socket_recv:49 is fixed.
// Data larger than a single TCP packet must arrive over multiple recv calls.
TEST(test_large_payload_recv_loop)
{
    auto p = make_loopback_pair();
    constexpr size_t N = 256 * 1024;
    std::vector<char> tx(N);
    for (size_t i = 0; i < N; ++i) tx[i] = static_cast<char>(i & 0x7F);

    std::thread sender([&]{
        REQUIRE(p.client.socket_send(tx.data(), N).e == Error::SUCCESS);
    });

    std::vector<char> rx(N);
    REQUIRE(p.server.socket_recv(rx.data(), N).e == Error::SUCCESS);
    sender.join();

    REQUIRE(rx == tx);
}

// Once the peer closes, recv must return an error instead of blocking.
TEST(test_recv_after_peer_close)
{
    auto p = make_loopback_pair();
    {
        SocketHandler tmp = std::move(p.client);
    }

    char buf[16];
    auto err = p.server.socket_recv(buf, sizeof(buf));
    REQUIRE(err.e != Error::SUCCESS);
}


// ========== Move construction ==========

TEST(test_move_ctor_transfers_ownership)
{
    auto p = make_loopback_pair();
    SocketHandler moved = std::move(p.server);

    const char msg[] = "via-moved";
    std::thread sender([&]{
        REQUIRE(p.client.socket_send(msg, sizeof(msg)).e == Error::SUCCESS);
    });

    char buf[sizeof(msg)] = {};
    REQUIRE(moved.socket_recv(buf, sizeof(buf)).e == Error::SUCCESS);
    sender.join();
    REQUIRE(std::string(buf) == "via-moved");
}

// After move, the source's socket must be INVALID_SOCKET and any send on it must fail.
TEST(test_move_ctor_clears_source)
{
    auto p = make_loopback_pair();
    SocketHandler moved = std::move(p.server);

    const char msg[] = "ghost";
    auto err = p.server.socket_send(msg, sizeof(msg));
    REQUIRE(err.e != Error::SUCCESS);
}

// Key invariant: destroying the moved-from object does not close the transferred socket.
// We prove the handle was not double-closed by ensuring moved can still recv.
TEST(test_no_double_close_after_move)
{
    auto p = make_loopback_pair();
    SocketHandler moved(SOCKET{INVALID_SOCKET}, sockaddr_in{});

    {
        SocketHandler temp = std::move(p.server);
        moved = std::move(temp);
    }

    const char msg[] = "alive";
    std::thread sender([&]{
        p.client.socket_send(msg, sizeof(msg));
    });

    char buf[sizeof(msg)] = {};
    REQUIRE(moved.socket_recv(buf, sizeof(buf)).e == Error::SUCCESS);
    sender.join();
}


// ========== Move assignment ==========

TEST(test_move_assign_releases_old_socket)
{
    auto p1 = make_loopback_pair();
    auto p2 = make_loopback_pair();

    p1.server = std::move(p2.server);

    const char msg[] = "p2";
    std::thread sender([&]{
        p2.client.socket_send(msg, sizeof(msg));
    });
    char buf[sizeof(msg)] = {};
    REQUIRE(p1.server.socket_recv(buf, sizeof(buf)).e == Error::SUCCESS);
    sender.join();
    REQUIRE(std::string(buf) == "p2");

    char tmp[4];
    auto err = p1.client.socket_recv(tmp, sizeof(tmp));
    REQUIRE(err.e != Error::SUCCESS);
}

// Self-assignment: a = std::move(a) must not close its own socket; otherwise all subsequent send/recv break.
TEST(test_self_move_assign_safe)
{
    auto p = make_loopback_pair();
    p.server = std::move(p.server);

    const char msg[] = "self";
    std::thread sender([&]{
        p.client.socket_send(msg, sizeof(msg));
    });
    char buf[sizeof(msg)] = {};
    REQUIRE(p.server.socket_recv(buf, sizeof(buf)).e == Error::SUCCESS);
    sender.join();
    REQUIRE(std::string(buf) == "self");
}


// ========== Destruction ==========

TEST(test_destructor_closes_socket)
{
    auto p = make_loopback_pair();
    {
        SocketHandler local = std::move(p.client);
    }

    char buf[16];
    auto err = p.server.socket_recv(buf, sizeof(buf));
    REQUIRE(err.e != Error::SUCCESS);
}


int main()
{
    WinsockGuard guard;
    std::cout << "Running SocketHandler tests...\n";

    RUN(test_basic_send_recv);
    RUN(test_large_payload_recv_loop);
    RUN(test_recv_after_peer_close);

    RUN(test_move_ctor_transfers_ownership);
    RUN(test_move_ctor_clears_source);
    RUN(test_no_double_close_after_move);

    RUN(test_move_assign_releases_old_socket);
    RUN(test_self_move_assign_safe);

    RUN(test_destructor_closes_socket);

    std::cout << "\nPassed: " << s_passed << ", Failed: " << s_failed << "\n";
    return s_failed == 0 ? 0 : 1;
}
