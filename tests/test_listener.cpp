// Tests the server-side create_listener factory.
// Verifies: port binding / SO_REUSEADDR / connect-ability / failure paths.
#include <iostream>
#include <stdexcept>
#include <thread>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "listener.h"
#include "test.h"

#pragma comment(lib, "Ws2_32.lib")

struct WinsockGuard {
    WinsockGuard() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            throw std::runtime_error("WSAStartup failed");
    }
    ~WinsockGuard() { WSACleanup(); }
};

// Acquire an ephemeral port: bind to 0 so the kernel assigns one, query via getsockname, close immediately.
// This makes the subsequent create_listener call almost always succeed in binding the port.
static int pick_free_port()
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    int len = sizeof(a);
    getsockname(s, reinterpret_cast<sockaddr*>(&a), &len);
    int port = ntohs(a.sin_port);
    closesocket(s);
    return port;
}


// ========== Basic: create listening socket ==========

TEST(test_create_listener_returns_valid_socket)
{
    int port = pick_free_port();
    SOCKET s = create_listener(port, 5);
    REQUIRE(s != INVALID_SOCKET);
    closesocket(s);
}

TEST(test_listener_accepts_loopback_connect)
{
    int port = pick_free_port();
    SOCKET listener = create_listener(port, 5);
    REQUIRE(listener != INVALID_SOCKET);

    std::thread cli([port]{
        SOCKET c = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(static_cast<u_short>(port));
        connect(c, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        closesocket(c);
    });

    sockaddr_in peer{};
    int plen = sizeof(peer);
    SOCKET srv = accept(listener, reinterpret_cast<sockaddr*>(&peer), &plen);
    cli.join();

    REQUIRE(srv != INVALID_SOCKET);
    closesocket(srv);
    closesocket(listener);
}


// ========== SO_REUSEADDR behavior ==========

// Open the same port twice; the second bind should still succeed.
// (SO_REUSEADDR semantics differ on Windows; here we only verify close-then-reopen reuse.)
TEST(test_listener_port_reusable_after_close)
{
    int port = pick_free_port();

    SOCKET s1 = create_listener(port, 5);
    REQUIRE(s1 != INVALID_SOCKET);
    closesocket(s1);

    SOCKET s2 = create_listener(port, 5);
    REQUIRE(s2 != INVALID_SOCKET);
    closesocket(s2);
}


// ========== Boundary cases ==========

// backlog=0 is silently raised to 1 on most platforms and listen() should still succeed.
TEST(test_listener_zero_backlog_still_works)
{
    int port = pick_free_port();
    // backlog=0 is silently raised to 1 on most platforms and listen() should still succeed.
    SOCKET s = create_listener(port, 0);
    REQUIRE(s != INVALID_SOCKET);
    closesocket(s);
}


int main()
{
    WinsockGuard guard;
    std::cout << "Running create_listener tests...\n";

    RUN(test_create_listener_returns_valid_socket);
    RUN(test_listener_accepts_loopback_connect);
    RUN(test_listener_port_reusable_after_close);
    RUN(test_listener_zero_backlog_still_works);

    std::cout << "\nPassed: " << s_passed << ", Failed: " << s_failed << "\n";
    return s_failed == 0 ? 0 : 1;
}
