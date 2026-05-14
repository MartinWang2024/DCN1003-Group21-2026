#include "listener.h"

#include <ws2tcpip.h>
#include <iostream>

SOCKET create_listener(int port, int backlog)
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET)
    {
        std::cerr << "create_listener: socket() failed: " << WSAGetLastError() << std::endl;
        return INVALID_SOCKET;
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(static_cast<u_short>(port));
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "create_listener: bind() failed: " << WSAGetLastError() << std::endl;
        closesocket(s);
        return INVALID_SOCKET;
    }

    if (listen(s, backlog) == SOCKET_ERROR)
    {
        std::cerr << "create_listener: listen() failed: " << WSAGetLastError() << std::endl;
        closesocket(s);
        return INVALID_SOCKET;
    }

    std::cout << "[*] Listening on port " << port << " (backlog=" << backlog << ")" << std::endl;
    return s;
}
