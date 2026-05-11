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
        UNKNOWN_ERR = 100
    };

    struct ErrorInfo
    {
        ErrorCode e = SUCCESS;      // 错误码
        std::string message = {0};  // 错误原因
    };


}
