#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")

constexpr int PORT    = 8888;
constexpr int BACKLOG = 10;
constexpr int BUF_LEN = 4096;

// 每个客户端连接的处理线程
void handle_client(SOCKET client_sock, sockaddr_in client_addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    int  port = ntohs(client_addr.sin_port);
    std::cout << "[+] Client connected: " << ip << ":" << port << std::endl;

    char buf[BUF_LEN];
    while (true) {
        int bytes = recv(client_sock, buf, BUF_LEN - 1, 0);
        if (bytes <= 0) {
            std::cout << "[-] Client disconnected: " << ip << ":" << port << std::endl;
            break;
        }
        buf[bytes] = '\0';
        std::string msg(buf);
        // 去除末尾换行
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n'))
            msg.pop_back();

        std::cout << "[" << ip << ":" << port << "] " << msg << std::endl;

        // 简单 ECHO 回复，后续替换为协议解析
        std::string reply = "ECHO: " + msg + "\r\n";
        send(client_sock, reply.c_str(), static_cast<int>(reply.size()), 0);
    }

    closesocket(client_sock);
}

int main() {
    // 初始化 Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 创建监听 socket
    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 允许地址复用
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // 绑定
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);
    if (bind(listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    // 监听
    if (listen(listen_sock, BACKLOG) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << WSAGetLastError() << std::endl;
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    std::cout << "[*] Server listening on port " << PORT << " ..." << std::endl;

    // Accept 循环
    while (true) {
        sockaddr_in client_addr{};
        int addr_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock,
                                    reinterpret_cast<sockaddr*>(&client_addr),
                                    &addr_len);
        if (client_sock == INVALID_SOCKET) {
            std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        // 为每个客户端创建独立线程
        std::thread(handle_client, client_sock, client_addr).detach();
    }

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
