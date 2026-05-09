
#pragma once
#include <cstdint>
#include <string>
#include <ws2tcpip.h>
#include "message.pb.h"

namespace Protocal {

    // 包头
    struct MsgHeader
    {
        uint32_t version;
        uint32_t cmd_type;
        uint32_t body_len;
        uint32_t hash[8] = {0};
        uint32_t reserved = {0};
    };
    // struct MsgBody
    // {
    //     uint16_t req_id;
    //     uint16_t timestamp;
    //     payload_t payload;
    // };
    //
    // typedef payload_t register_request_t;


    bool SendMessage(SOCKET sock, const google::protobuf::Message& msg);
    bool ReceiveMessage(SOCKET sock, google::protobuf::Message& msg);
}

