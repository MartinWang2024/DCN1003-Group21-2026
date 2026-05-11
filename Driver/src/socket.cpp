#include "socket.h"

TcpSocket::Socket::Socket(SOCKET client_sock, sockaddr_in client_addr)
{
    this->socket = client_sock;
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    port = ntohs(client_addr.sin_port);
}

Error::ErrorInfo socket_send(SOCKET socket, const void* send_data, size_t data_len)
{
    Error::ErrorInfo err;
    const char* data_ptr = static_cast<const char*>(send_data);
    size_t total_sent = 0;
    while (total_sent < data_len)
    {
        int sent = send(
            socket,
            data_ptr + total_sent,
            static_cast<int>(data_len - total_sent),
            0
            );

        if (sent <= 0) err.e = Error::SOCKET_ERR; // 发生错误
        total_sent += sent;
    }
    err.message = "Sent " + std::to_string(total_sent) + " bytes successfully.";
    return err;
}

Error::ErrorInfo socket_recv(SOCKET socket, void* recv_data, const size_t data_len)
{
    Error::ErrorInfo err;
    char* data_ptr = static_cast<char*>(recv_data);
    size_t total_recv = 0;
    // 确保按预期字节数量接收数据
    while (total_recv < data_len)
    {
        int recv_byte = recv(
            socket,
            data_ptr,
            static_cast<int>(data_len - total_recv),
            0
            );
        if (recv_byte <= 0)
        {
            err.e = Error::SOCKET_ERR;
            err.message = "Not enough length data receive.";
            break;
        };
        total_recv += recv_byte;
    }
    return err;
}