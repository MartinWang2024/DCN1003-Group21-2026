#pragma once
#include <cstdint>
#include <string>
#include <ws2tcpip.h>
#include "message.pb.h"
#include "openssl.h"
#include "error.h"
#include "SocketHandler.h"
#include "cmd_type.h"

// TODO 接收解码
// TODO 测试发包收包代码正常工作
// TODO 划定指令数量及内容
// TODO 登录逻辑
// TODO 课程查询逻辑


static constexpr uint32_t MAX_BODY = 16 * 1024 * 1024;

namespace Protocal {
    namespace detail {
        inline std::atomic<uint32_t> g_req_id_counter{1};
    }

    // 包头
    struct MsgHeader
    {
        uint32_t version;		// 协议版本号
        uint32_t body_len;		// 加密后有效字段长度
        uint8_t iv[16];			// AES初始向量
        uint8_t mac[32] = {0};	// 消息认证码
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
    Error::ErrorInfo Package_receive(TcpSocket::SocketHandler &sh, google::protobuf::Message& msg);
}

