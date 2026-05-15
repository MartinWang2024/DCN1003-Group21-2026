// Tests the Package_send / Package_receive pipeline: protobuf -> HMAC -> AES -> socket.
// Use loopback (127.0.0.1) to build a real TCP pair so the sender and receiver are wired to each other.
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
// Build a loopback socket pair already connected via TCP.
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

    // Client connect and server accept must run concurrently; otherwise they block each other.
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
        throw std::runtime_error("readAppKey failed - ensure app.key exists in the working directory");
}

// Convenience wrapper for small send/recv: small packets return immediately on loopback, no async needed.
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
// CASE 1: basic round-trip - verify cmd_type and payload.
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
// CASE 2: binary payload (containing \0) - must not be truncated as a C-string.
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
// CASE 3: 64KB payload - verifies socket_recv loops correctly (catches fragmented-recv bugs).
// Large packets require concurrency: send may block on a full kernel buffer and depends on the peer's recv to drain it.
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
// CASE 4: bidirectional - full client -> server -> client request/response flow.
// Must run concurrently: server send depends on the client recv freeing buffer space first.
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
// CASE 5: req_id monotonic increase - verifies it is no longer a hard-coded literal.
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
// CASE 6: decryption with a wrong key - must fail (synchronous send avoids races with the key flip).
// ─────────────────────────────────────────────
TEST(test_receive_rejects_wrong_key) {
    auto pair = make_loopback_pair();

    // First push the full ciphertext into the TCP buffer using the original key.
    REQUIRE(send_str(pair.client, "hello world", 0x5005).e == Error::SUCCESS);

    // Then flip the key; ciphertext already on the wire is unaffected.
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
