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

            // 不准拷贝
            SocketHandler(const SocketHandler &) = delete;
            SocketHandler& operator=(const SocketHandler&) = delete;

            // 允许资源所有权转移
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
                if (socket != INVALID_SOCKET)    // 防止 closesocket(0) 之类
                    closesocket(socket);
            }

            // 通过 accept 函数实例化 Socket 对象
            SocketHandler(SOCKET client_sock, sockaddr_in client_addr);

            /**
             * 发送数据包
             * @param socket socket句柄
             * @param send_data 待发送数据的指针
             * @param data_len 数据长度
             * @return err结构体
             */
            Error::ErrorInfo socket_send(const void* send_data, size_t data_len);

            /**
             * 接收数据包
             * @param socket socket句柄
             * @param recv_data 接收数据缓冲区句柄
             * @param data_len 需要接收的长度
             * @return err结构体
             */
            Error::ErrorInfo socket_recv(void* recv_data, size_t data_len);
    };
}
