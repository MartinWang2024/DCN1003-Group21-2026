#include "connect_to.h"

#include <ws2tcpip.h>
#include <iostream>

using TcpSocket::SocketHandler;

SocketHandler connect_to(const std::string& host, int port, Error::ErrorInfo& out_err)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(static_cast<u_short>(port));

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
    {
        out_err.e = Error::SOCKET_ERR;
        out_err.message = "Invalid host IP: " + host;
        sockaddr_in dummy{};
        return SocketHandler(INVALID_SOCKET, dummy);
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        out_err.e = Error::SOCKET_ERR;
        out_err.message = "socket() failed: " + std::to_string(WSAGetLastError());
        sockaddr_in dummy{};
        return SocketHandler(INVALID_SOCKET, dummy);
    }

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        out_err.e = Error::SOCKET_ERR;
        out_err.message = "connect() failed: " + std::to_string(WSAGetLastError());
        closesocket(s);
        sockaddr_in dummy{};
        return SocketHandler(INVALID_SOCKET, dummy);
    }

    out_err.e = Error::SUCCESS;
    return SocketHandler(s, addr);
}
