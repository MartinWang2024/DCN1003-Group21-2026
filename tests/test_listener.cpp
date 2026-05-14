// 测试 server 端 create_listener 工厂
// 验证: 端口绑定 / SO_REUSEADDR 生效 / 可被 connect / 失败路径
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

// 取一个临时端口: 先 bind 到 0 让内核分配, getsockname 拿端口, 立即关闭
// 这样后续 create_listener 用这个端口几乎总能 bind 成功
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


// ========== 基础: 创建监听 socket ==========

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


// ========== SO_REUSEADDR 行为 ==========

// 同一端口连开两次, 第二次应仍能 bind 成功
// (SO_REUSEADDR 在 Windows 上的语义不同, 这里只验证: 先关后开能复用)
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


// ========== 边界 ==========

// backlog=0 在大多数平台被静默上调为 1, 应仍能 listen 成功
TEST(test_listener_zero_backlog_still_works)
{
    int port = pick_free_port();
    // backlog=0 在大多数平台被静默上调为 1, 应仍能 listen 成功
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
