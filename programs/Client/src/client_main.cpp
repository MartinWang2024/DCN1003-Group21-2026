#include "client_main.h"
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

std::atomic<bool> running{true};

// 接收线程：持续读取服务器消息并打印
void recv_thread(SOCKET sock) {
    char buf[BUF_LEN];
    while (running) {
        int bytes = recv(sock, buf, BUF_LEN - 1, 0);
        if (bytes <= 0) {
            std::cout << "\n[!] Server disconnected." << std::endl;
            running = false;
            break;
        }
        buf[bytes] = '\0';
        std::cout << "[Server] " << buf;
    }
}

int main() {
    // 初始化 Winsock
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // 创建 socket
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 连接服务器
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    // 将字符串 IP 转换为二进制形式,并存储在 server_addr.sin_addr 中，inet_pton 函数返回 1 表示成功，0 表示输入不是有效的 IP 地址，-1 表示发生错误
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) != 1) {
        std::cerr << "Invalid server IP." << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr))
        == SOCKET_ERROR) {
        std::cerr << "connect() failed: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "[*] Connected to " << SERVER_IP << ":" << PORT << std::endl;
    std::cout << "[*] Type a message and press Enter. Type 'quit' to exit.\n" << std::endl;

    // 启动接收线程
    std::thread t(recv_thread, sock);

    // 主线程负责读取用户输入并发送
    std::string line;
    while (running && std::getline(std::cin, line)) {
        std::string out = line + "\r\n";
        if (send(sock, out.c_str(), static_cast<int>(out.size()), 0) == SOCKET_ERROR) {
            std::cerr << "send() failed: " << WSAGetLastError() << std::endl;
            break;
        }

        if (line == "quit") {
            break;
        }
    }

    shutdown(sock, SD_SEND); // 告知服务端不再发送，等待服务端关闭连接
    if (t.joinable()) t.join();
    closesocket(sock);
    running = false;

    WSACleanup();
    return 0;
}
