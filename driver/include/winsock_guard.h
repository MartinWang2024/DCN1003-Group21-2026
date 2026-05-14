#pragma once
#include "error.h"

namespace TcpSocket
{
    // Winsock 全局生命周期 RAII 管理
    // 进程入口构造一次, 析构时自动 WSACleanup
    // 失败时通过 last_error() 暴露错误码, 不抛异常
    class WinsockGuard
    {
    public:
        WinsockGuard();
        ~WinsockGuard();

        WinsockGuard(const WinsockGuard&) = delete;
        WinsockGuard& operator=(const WinsockGuard&) = delete;
        WinsockGuard(WinsockGuard&&) = delete;
        WinsockGuard& operator=(WinsockGuard&&) = delete;

        bool ok() const { return err_.e == Error::SUCCESS; }
        const Error::ErrorInfo& last_error() const { return err_; }

    private:
        Error::ErrorInfo err_;
        bool initialized_ = false;
    };
}
