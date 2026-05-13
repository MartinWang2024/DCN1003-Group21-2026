// 测试 Package_send / Package_receive 全流程：protobuf → HMAC → AES → socket
// 通过 loopback (127.0.0.1) 构造一对真实 TCP 连接，让发收两端对接自己
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
// 建立 loopback socket pair，二者已通过 TCP 连上
// ─────────────────────────────────────────────
struct SockPair {
    TcpSocket::SocketHandler server;
    TcpSocket::SocketHandler client;
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

    // 客户端 connect 与服务端 accept 必须并发，否则互相阻塞
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

static void ensure_key_loaded() {
    if (openssl::readAppKey(openssl::key, "app.key").e != Error::SUCCESS)
        throw std::runtime_error("readAppKey failed - 请确保运行目录下有 app.key");
}

// 小包发收的便利封装：loopback 上小包 send 立即返回，无需异步
static Error::ErrorInfo send_str(TcpSocket::SocketHandler& sh,
                                  const std::string& payload,
                                  uint32_t cmd_type) {
    return Protocal::Package_send(
        sh,
        reinterpret_cast<const u_char*>(payload.data()),
        payload.size(),
        cmd_type);
}

// ─────────────────────────────────────────────
// CASE 1: 基本回环 —— 校验 cmd_type 与 payload
// ─────────────────────────────────────────────
TEST(test_send_receive_basic_roundtrip) {
    auto pair = make_loopback_pair();
    const std::string payload = "hello world";
    const uint32_t cmd = 0x1001;

    REQUIRE(send_str(pair.client, payload, cmd).e == Error::SUCCESS);

    MsgBody body;
    REQUIRE(Protocal::Package_receive(pair.server, body).e == Error::SUCCESS);
    REQUIRE(body.cmd_type() == cmd);
    REQUIRE(body.payload().json_size() == 1);
    REQUIRE(body.payload().json(0) == payload);
}

// ─────────────────────────────────────────────
// CASE 2: 二进制 payload（含 \0）—— 验证不会被字符串截断
// ─────────────────────────────────────────────
TEST(test_send_receive_binary_payload_with_nulls) {
    auto pair = make_loopback_pair();
    const std::string payload{'A', '\0', 'B', '\0', 'C'};

    REQUIRE(send_str(pair.client, payload, 0x2002).e == Error::SUCCESS);

    MsgBody body;
    REQUIRE(Protocal::Package_receive(pair.server, body).e == Error::SUCCESS);
    REQUIRE(body.payload().json_size() == 1);
    REQUIRE(body.payload().json(0).size() == 5);
    REQUIRE(body.payload().json(0) == payload);
}

// ─────────────────────────────────────────────
// CASE 3: 64KB payload —— 验证 socket_recv 循环正确（暴露分片接收 bug）
// 大包需要并发：send 可能因内核 buffer 满而阻塞，必须由对端 recv 同步消费
// ─────────────────────────────────────────────
TEST(test_send_receive_large_payload) {
    auto pair = make_loopback_pair();

    std::string payload(64 * 1024, '\0');
    for (size_t i = 0; i < payload.size(); ++i)
        payload[i] = static_cast<char>('A' + (i % 26));

    Error::ErrorInfo send_err;
    std::thread sender([&] {
        send_err = send_str(pair.client, payload, 0x3003);
    });

    MsgBody body;
    Error::ErrorInfo recv_err = Protocal::Package_receive(pair.server, body);
    sender.join();

    REQUIRE(send_err.e == Error::SUCCESS);
    REQUIRE(recv_err.e == Error::SUCCESS);
    REQUIRE(body.payload().json(0) == payload);
}

// ─────────────────────────────────────────────
// CASE 4: 双向 —— client → server → client 完整请求/响应流程
// 必须并发：服务端的 send 依赖客户端先 receive 才能腾出 buffer
// ─────────────────────────────────────────────
TEST(test_send_receive_bidirectional) {
    auto pair = make_loopback_pair();

    Error::ErrorInfo s_recv_err, s_send_err;
    std::thread server_thread([&] {
        MsgBody req;
        s_recv_err = Protocal::Package_receive(pair.server, req);
        if (s_recv_err.e != Error::SUCCESS) return;

        const std::string reply = "RESULT:" + req.payload().json(0);
        s_send_err = send_str(pair.server, reply, 0xFF00);
    });

    REQUIRE(send_str(pair.client, "QUERY CS101", 0x0101).e == Error::SUCCESS);

    MsgBody resp;
    Error::ErrorInfo c_recv_err = Protocal::Package_receive(pair.client, resp);
    server_thread.join();

    REQUIRE(s_recv_err.e == Error::SUCCESS);
    REQUIRE(s_send_err.e == Error::SUCCESS);
    REQUIRE(c_recv_err.e == Error::SUCCESS);
    REQUIRE(resp.cmd_type() == 0xFF00);
    REQUIRE(resp.payload().json(0) == "RESULT:QUERY CS101");
}

// ─────────────────────────────────────────────
// CASE 5: req_id 单调递增 —— 验证不再是硬编码字面量
// ─────────────────────────────────────────────
TEST(test_send_req_id_is_monotonic) {
    auto pair = make_loopback_pair();

    for (int i = 0; i < 3; ++i)
        REQUIRE(send_str(pair.client, "x", 0x4004).e == Error::SUCCESS);

    uint32_t ids[3] = {0};
    for (int i = 0; i < 3; ++i) {
        MsgBody body;
        REQUIRE(Protocal::Package_receive(pair.server, body).e == Error::SUCCESS);
        ids[i] = body.req_id();
    }

    REQUIRE(ids[1] == ids[0] + 1);
    REQUIRE(ids[2] == ids[1] + 1);
}

// ─────────────────────────────────────────────
// CASE 6: 错误 key 解密 —— 必须失败（同步发送避免与 key 翻转产生竞态）
// ─────────────────────────────────────────────
TEST(test_receive_rejects_wrong_key) {
    auto pair = make_loopback_pair();

    // 先用原始 key 把密文完整送进 TCP 缓冲区
    REQUIRE(send_str(pair.client, "hello world", 0x5005).e == Error::SUCCESS);

    // 此后再翻转 key，已发出的密文不受影响
    unsigned char saved_key[32];
    memcpy(saved_key, openssl::key, 32);
    openssl::key[0] ^= 0xFF;

    MsgBody body;
    Error::ErrorInfo recv_err = Protocal::Package_receive(pair.server, body);

    memcpy(openssl::key, saved_key, 32);
    REQUIRE(recv_err.e != Error::SUCCESS);
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
        RUN(test_receive_rejects_wrong_key);

        std::cout << "\n--- Result: " << s_passed << " passed, "
                  << s_failed << " failed ---\n";
        return s_failed == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 2;
    }
}
