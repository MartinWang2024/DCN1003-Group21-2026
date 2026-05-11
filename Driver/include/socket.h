#pragma once
// #include <psdk_inc/_socket_types.h>
#include "error.h"
#include <ws2tcpip.h>

namespace TcpSocket
{
    class Socket
    {
        private:
            SOCKET socket;
            char ip[INET_ADDRSTRLEN];
            int port = -1;

        public:
            // 通过 accept 函数实例化 Socket 对象
            Socket(SOCKET client_sock, sockaddr_in client_addr);
            ~Socket();

            /**
             * 发送数据包
             * @param socket socket句柄
             * @param send_data 待发送数据的指针
             * @param data_len 数据长度
             * @return err结构体
             */
            Error::ErrorInfo package_send(SOCKET socket, const void* send_data, size_t data_len);

            /**
             * 接收数据包
             * @param socket socket句柄
             * @param recv_data 接收数据缓冲区句柄
             * @param data_len 需要接收的长度
             * @return err结构体
             */
            Error::ErrorInfo package_recv(SOCKET socket, void* recv_data, size_t data_len);
    };
}
