#pragma once
#include "error.h"

namespace TcpSocket
{
    // RAII guard for Winsock global lifecycle.
    // Construct once at process entry; destructor calls WSACleanup.
    // On failure exposes error code via last_error() (no exceptions).
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
