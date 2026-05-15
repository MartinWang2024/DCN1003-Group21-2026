#pragma once
// #include <psdk_inc/_socket_types.h>
#include "error.h"
#include <ws2tcpip.h>
#include <cstring>

namespace TcpSocket
{
    class SocketHandler
    {
        private:
            SOCKET socket;
            char ip[INET_ADDRSTRLEN];
            int port = -1;

        public:

            // Non-copyable
            SocketHandler(const SocketHandler &) = delete;
            SocketHandler& operator=(const SocketHandler&) = delete;

            // Movable (socket ownership transfer)
            SocketHandler(SocketHandler&& other) noexcept
                : socket(other.socket), port(other.port)
            {
                std::memcpy(ip, other.ip, sizeof(ip));
                other.socket = INVALID_SOCKET;
                other.ip[0] = '\0';
                other.port = -1;
            }
            SocketHandler& operator=(SocketHandler&& other) noexcept;


            ~SocketHandler() {
                if (socket != INVALID_SOCKET)    // guard against e.g. closesocket(0)
                    closesocket(socket);
            }

            // Constructed via accept()
            SocketHandler(SOCKET client_sock, sockaddr_in client_addr);

            /**
             * Send data with loop until all bytes are written.
             * @param send_data Pointer to data buffer
             * @param data_len  Number of bytes to send
             * @return ErrorInfo (SUCCESS on complete send)
             */
            Error::ErrorInfo socket_send(const void* send_data, size_t data_len);

            /**
             * Receive exactly data_len bytes (loops until full).
             * @param recv_data Buffer to fill
             * @param data_len  Exact number of bytes to receive
             * @return ErrorInfo (SUCCESS on complete receive)
             */
            Error::ErrorInfo socket_recv(void* recv_data, size_t data_len);
    };
}
