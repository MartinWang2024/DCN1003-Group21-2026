#pragma once
#include <string>

namespace Error
{
    enum ErrorCode
    {
        SUCCESS = 0,
        SOCKET_ERR = 1,
        BIND_ERR = 2,
        LISTEN_ERR = 3,
        ACCEPT_ERR = 4,
        SEND_ERR = 5,
        RECV_ERR = 6,
        READ_ERR = 7,
        MAC_CALC_ERR = 8,
        MAC_CHECK_ERR = 9,
        IV_CALC_ERR = 10,
        ENCRYPT_ERR = 11,
        DECRYPT_ERR = 12,
        PROTO_CODING_ERR = 13,
        PROTO_PARSE_ERR = 14,
        UNKNOWN_ERR = 65535,
    };

    struct ErrorInfo
    {
        ErrorCode e = SUCCESS;      // 错误码
        std::string message = {0};  // 错误原因
    };


}
