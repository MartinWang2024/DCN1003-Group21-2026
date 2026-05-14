#include "winsock_guard.h"

#include <winsock2.h>

#include "log.h"

using namespace TcpSocket;

WinsockGuard::WinsockGuard()
{
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0)
    {
        err_.e = Error::SOCKET_ERR;
        err_.message = "WSAStartup failed";
        print_log(error, "WinsockGuard: WSAStartup failed, code=%d", rc);
        return;
    }
    initialized_ = true;
}

WinsockGuard::~WinsockGuard()
{
    if (initialized_) WSACleanup();
}
