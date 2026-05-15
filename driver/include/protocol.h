#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <ws2tcpip.h>
#include "message.pb.h"
#include "openssl.h"
#include "error.h"
#include "SocketHandler.h"
#include "cmd_type.h"

static constexpr uint32_t MAX_BODY = 16 * 1024 * 1024;

namespace Protocal {
    namespace detail {
        inline std::atomic<uint32_t> g_req_id_counter{1};
    }

    // Wire header (plaintext)
    struct MsgHeader_t
    {
        uint32_t version{};
        uint32_t body_len{};       // ciphertext length
        uint8_t iv[16]{};          // AES IV (random per packet)
        uint8_t mac[32] = {0};     // HMAC-SHA256 over plaintext MsgBody
    };
    // struct MsgBody
    // {
    //     uint16_t req_id;
    //     uint16_t timestamp;
    //     payload_t payload;
    // };
    //
    // typedef payload_t register_request_t;


    Error::ErrorInfo Package_send(TcpSocket::SocketHandler &sh, const unsigned char* msg, size_t msg_len, uint32_t cmd_type);
    Error::ErrorInfo Package_send(TcpSocket::SocketHandler &sh, const std::vector<std::string>& fields, uint32_t cmd_type);
    Error::ErrorInfo Package_receive(TcpSocket::SocketHandler &sh, google::protobuf::Message& msg);
}

