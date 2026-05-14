// 测试 SocketHandler 的 RAII / 移动语义 / send-recv 循环正确性
// 通过 loopback (127.0.0.1) 构造真实 TCP socket pair 进行行为验证
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
// 一次性 Winsock 初始化 / 清理
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
// 建立 loopback socket pair, 直接返回两端 SocketHandler
// 与 test_protocol 同款工具, 拷贝在此避免跨文件依赖
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


// ========== 基础 send / recv ==========

TEST(test_basic_send_recv)
{
    auto p = make_loopback_pair();
    const char msg[] = "hello world";
    REQUIRE(p.client.socket_send(msg, sizeof(msg)).e == Error::SUCCESS);

    char buf[sizeof(msg)] = {};
    REQUIRE(p.server.socket_recv(buf, sizeof(buf)).e == Error::SUCCESS);
    REQUIRE(std::string(buf) == "hello world");
}

// 验证 socket_recv 第 49 行 data_ptr+total_recv bug 已修复
// 大于 TCP 单包尺寸的数据必然分多次 recv 返回
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

// 远端断开后 recv 应返回错误而非阻塞
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


// ========== 移动构造 ==========

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

// 移动后源对象的 socket 应为 INVALID_SOCKET, 调用其 send 必然失败
TEST(test_move_ctor_clears_source)
{
    auto p = make_loopback_pair();
    SocketHandler moved = std::move(p.server);

    const char msg[] = "ghost";
    auto err = p.server.socket_send(msg, sizeof(msg));
    REQUIRE(err.e != Error::SUCCESS);
}

// 验证关键不变量: 移动后源对象析构不会关闭已转移的 socket
// 通过让 moved 仍能正常 recv 来证明 socket 句柄未被双重 close
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


// ========== 移动赋值 ==========

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

// 自赋值: a = std::move(a) 不能关闭自己的 socket, 否则后续 send/recv 全废
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


// ========== 析构 ==========

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
