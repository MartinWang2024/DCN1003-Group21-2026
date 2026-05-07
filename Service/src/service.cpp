#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>

#include "protocol.h"
#include "database.h"

#pragma comment(lib, "ws2_32.lib")

constexpr int PORT    = 8888;
constexpr int BACKLOG = 10;
constexpr int BUF_LEN = 4096;

dcn_protocol::CourseStore g_store;

// 每个客户端连接的处理线程
void handle_client(SOCKET client_sock, sockaddr_in client_addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
    int  port = ntohs(client_addr.sin_port);
    std::cout << "[+] Client connected: " << ip << ":" << port << std::endl;

    char buf[BUF_LEN];
    std::string pending;
    while (true) {
        int bytes = recv(client_sock, buf, BUF_LEN - 1, 0);
        if (bytes <= 0) {
            std::cout << "[-] Client disconnected: " << ip << ":" << port << std::endl;
            break;
        }
        buf[bytes] = '\0';
        pending.append(buf, bytes);

        size_t pos = 0;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }

            std::cout << "[" << ip << ":" << port << "] " << line << std::endl;
            auto replies = g_store.process_command(line);
            for (const auto& r : replies) {
                std::string out = r + "\r\n";
                send(client_sock, out.c_str(), static_cast<int>(out.size()), 0);
            }
        }
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
