// 测试 Package_send / Package_receive 全流程：protobuf → HMAC → AES → socket
// 通过 loopback (127.0.0.1) 构造一对真实 TCP 连接，让发收两端对接自己
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "protocol.h"
#include "../third_party/openssl/src/openssl.h"
#include "test.h"

#pragma comment(lib, "Ws2_32.lib")

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
// 建立 loopback socket pair
// 返回 (server_handler, client_handler)，二者已通过 TCP 连上
// ─────────────────────────────────────────────
struct SockPair {
    TcpSocket::SocketHandler server;
    TcpSocket::SocketHandler client;
};

static SockPair make_loopback_pair() {
    // 1. 监听端
    SOCKET listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == INVALID_SOCKET) throw std::runtime_error("listener socket failed");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // OS 自动分配端口

    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
        throw std::runtime_error("bind failed");
    if (listen(listener, 1) == SOCKET_ERROR)
        throw std::runtime_error("listen failed");

    int alen = sizeof(addr);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &alen) == SOCKET_ERROR)
        throw std::runtime_error("getsockname failed");

    // 2. 客户端连接（异步，避免和 accept 互相阻塞）
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
        TcpSocket::SocketHandler(srv, peer),
        TcpSocket::SocketHandler(cli, cli_addr),
    };
}

// ─────────────────────────────────────────────
// 全局前置：保证 openssl::key 已加载
// ─────────────────────────────────────────────
static void ensure_key_loaded() {
    if (openssl::readAppKey(openssl::key, "app.key").e != Error::SUCCESS)
        throw std::runtime_error("readAppKey failed - 请确保运行目录下有 app.key");
}

// ─────────────────────────────────────────────
// CASE 1: 基本回环 —— 客户端发，服务端收，校验 cmd_type 与 payload
// ─────────────────────────────────────────────
TEST(test_send_receive_basic_roundtrip) {
    auto pair = make_loopback_pair();

    const char* payload = "hello world";
    const uint32_t cmd  = 0x1001;
    Error::ErrorInfo send_err;

    // 发送线程
    std::thread sender([&] {
        send_err = Protocal::Package_send(
            pair.client,
            reinterpret_cast<const u_char*>(payload),
            std::strlen(payload),
            cmd);
    });

    // 主线程做接收
    MsgBody body;
    Error::ErrorInfo recv_err = Protocal::Package_receive(pair.server, body);
    sender.join();

    REQUIRE(send_err.e == Error::SUCCESS);
    REQUIRE(recv_err.e == Error::SUCCESS);
    REQUIRE(body.cmd_type() == cmd);
    REQUIRE(body.payload().json_size() == 1);
    REQUIRE(body.payload().json(0) == std::string(payload));
}

// ─────────────────────────────────────────────
// CASE 2: 二进制 payload（含 \0）—— 验证不会被字符串截断
// ─────────────────────────────────────────────
TEST(test_send_receive_binary_payload_with_nulls) {
    auto pair = make_loopback_pair();

    std::string payload;
    payload.push_back('A');
    payload.push_back('\0');
    payload.push_back('B');
    payload.push_back('\0');
    payload.push_back('C');

    Error::ErrorInfo send_err, recv_err;

    std::thread sender([&] {
        send_err = Protocal::Package_send(
            pair.client,
            reinterpret_cast<const u_char*>(payload.data()),
            payload.size(),
            0x2002);
    });

    MsgBody body;
    recv_err = Protocal::Package_receive(pair.server, body);
    sender.join();

    REQUIRE(send_err.e == Error::SUCCESS);
    REQUIRE(recv_err.e == Error::SUCCESS);
    REQUIRE(body.payload().json_size() == 1);
    REQUIRE(body.payload().json(0) == payload);  // 完整 5 字节
    REQUIRE(body.payload().json(0).size() == 5);
}

// ─────────────────────────────────────────────
// CASE 3: 大 payload —— 验证分片接收正确（暴露 socket_recv 循环 bug）
// ─────────────────────────────────────────────
TEST(test_send_receive_large_payload) {
    auto pair = make_loopback_pair();

    // 构造 64KB 的 payload，强制 TCP 分片
    std::string payload(64 * 1024, '\0');
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>(i & 0xFF);

    Error::ErrorInfo send_err, recv_err;
    std::thread sender([&] {
        send_err = Protocal::Package_send(
            pair.client,
            reinterpret_cast<const u_char*>(payload.data()),
            payload.size(),
            0x3003);
    });

    MsgBody body;
    recv_err = Protocal::Package_receive(pair.server, body);
    sender.join();

    REQUIRE(send_err.e == Error::SUCCESS);
    REQUIRE(recv_err.e == Error::SUCCESS);
    REQUIRE(body.payload().json_size() == 1);
    REQUIRE(body.payload().json(0) == payload);
}

// ─────────────────────────────────────────────
// CASE 4: 双向 —— A 发给 B, B 回给 A
// ─────────────────────────────────────────────
TEST(test_send_receive_bidirectional) {
    auto pair = make_loopback_pair();

    Error::ErrorInfo c2s_send, s_recv, s2c_send, c_recv;

    std::thread server_thread([&] {
        MsgBody req;
        s_recv = Protocal::Package_receive(pair.server, req);
        if (s_recv.e != Error::SUCCESS) return;

        const std::string reply = "RESULT:" + req.payload().json(0);
        s2c_send = Protocal::Package_send(
            pair.server,
            reinterpret_cast<const u_char*>(reply.data()),
            reply.size(),
            0xFF00);
    });

    const char* req_payload = "QUERY CS101";
    c2s_send = Protocal::Package_send(
        pair.client,
        reinterpret_cast<const u_char*>(req_payload),
        std::strlen(req_payload),
        0x0101);

    MsgBody resp;
    c_recv = Protocal::Package_receive(pair.client, resp);
    server_thread.join();

    REQUIRE(c2s_send.e == Error::SUCCESS);
    REQUIRE(s_recv.e   == Error::SUCCESS);
    REQUIRE(s2c_send.e == Error::SUCCESS);
    REQUIRE(c_recv.e   == Error::SUCCESS);
    REQUIRE(resp.cmd_type() == 0xFF00);
    REQUIRE(resp.payload().json(0) == "RESULT:QUERY CS101");
}

// ─────────────────────────────────────────────
// CASE 5: req_id 单调递增 —— 验证不再是硬编码字面量
// ─────────────────────────────────────────────
TEST(test_send_req_id_is_monotonic) {
    auto pair = make_loopback_pair();
    const char* payload = "x";

    std::thread sender([&] {
        for (int i = 0; i < 3; ++i) {
            Protocal::Package_send(
                pair.client,
                reinterpret_cast<const u_char*>(payload),
                1,
                0x4004);
        }
    });

    uint32_t ids[3] = {0};
    for (int i = 0; i < 3; ++i) {
        MsgBody body;
        REQUIRE(Protocal::Package_receive(pair.server, body).e == Error::SUCCESS);
        ids[i] = body.req_id();
    }
    sender.join();

    REQUIRE(ids[1] == ids[0] + 1);
    REQUIRE(ids[2] == ids[1] + 1);
}

// ─────────────────────────────────────────────
// CASE 6: 篡改密文 —— MAC 校验应失败
// ─────────────────────────────────────────────
TEST(test_receive_rejects_tampered_ciphertext) {
    auto pair = make_loopback_pair();

    const char* payload = "hello world";
    Error::ErrorInfo send_err;

    // 临时翻转接收端的 key，让 AES 解密 / MAC 校验失败
    std::thread sender([&] {
        send_err = Protocal::Package_send(
            pair.client,
            reinterpret_cast<const u_char*>(payload),
            std::strlen(payload),
            0x5005);
    });

    // 临时改 key（注意：这破坏了全局状态，测试结束后要还原）
    unsigned char saved_key[32];
    memcpy(saved_key, openssl::key, 32);
    openssl::key[0] ^= 0xFF;  // 翻转 1 bit

    MsgBody body;
    Error::ErrorInfo recv_err = Protocal::Package_receive(pair.server, body);

    // 还原 key
    memcpy(openssl::key, saved_key, 32);
    sender.join();

    REQUIRE(send_err.e == Error::SUCCESS);
    REQUIRE(recv_err.e != Error::SUCCESS);  // 必须失败（解密 or MAC）
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    try {
        WinsockGuard wsa;
        ensure_key_loaded();

        std::cout << "=== Protocol send/receive integration tests ===\n";
        RUN(test_send_receive_basic_roundtrip);
        RUN(test_send_receive_binary_payload_with_nulls);
        RUN(test_send_receive_large_payload);
        RUN(test_send_receive_bidirectional);
        RUN(test_send_req_id_is_monotonic);
        RUN(test_receive_rejects_tampered_ciphertext);

        std::cout << "\n--- Result: " << s_passed << " passed, "
                  << s_failed << " failed ---\n";
        return s_failed == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 2;
    }
}
