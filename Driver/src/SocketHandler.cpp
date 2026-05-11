#include "SocketHandler.h"

#include "log.h"
using namespace TcpSocket;

SocketHandler::SocketHandler(SOCKET client_sock, sockaddr_in client_addr)
{
    this->socket = client_sock;
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    port = ntohs(client_addr.sin_port);
}

Error::ErrorInfo SocketHandler::socket_send(const void* send_data, size_t data_len)
{
    Error::ErrorInfo err;
    const char* data_ptr = static_cast<const char*>(send_data);
    size_t total_sent = 0;
    while (total_sent < data_len)
    {
        int sent = send(
            this->socket,
            data_ptr + total_sent,
            static_cast<int>(data_len - total_sent),
            0);

        if (sent <= 0)
        {
            err.e = Error::SOCKET_ERR;
            err.message = "send failed";
            print_log(err, debug);
            return err;
        }
        total_sent += sent;
    }
    return err;
}

Error::ErrorInfo SocketHandler::socket_recv(void* recv_data, const size_t data_len)
{
    Error::ErrorInfo err;
    char* data_ptr = static_cast<char*>(recv_data);
    size_t total_recv = 0;
    // 确保按预期字节数量接收数据
    while (total_recv < data_len)
    {
        int recv_byte = recv(
            this->socket,
            data_ptr,
            static_cast<int>(data_len - total_recv),
            0
            );
        if (recv_byte <= 0)
        {
            err.e = Error::SOCKET_ERR;
            err.message = "Not enough length data receive.";
            print_log(err, debug);
            return err;
        };
        total_recv += recv_byte;
    }
    return err;
}