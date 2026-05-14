#include "server_main.h"
#include "listener.h"
#include "winsock_guard.h"
#include <ws2tcpip.h>
#include <iostream>
#include <thread>

bool send_line(SOCKET sock, const std::string& text) {
    std::string out = text + "\r\n";
    const char* data = out.c_str();
    int total_sent = 0;
    int to_send = static_cast<int>(out.size());

    while (total_sent < to_send) {
        int sent = send(sock, data + total_sent, to_send - total_sent, 0);
        if (sent == SOCKET_ERROR || sent == 0) {
            return false;
        }
        total_sent += sent;
    }
    return true;
}

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
            if (line == "quit" || line == "exit") {
                if (!send_line(client_sock, "BYE")) {
                    break;
                }
                std::cout << "[*] Client requested close: " << ip << ":" << port << std::endl;
                closesocket(client_sock);
                return;
            }

            if (line == "ping" || line == "PING") {
                if (!send_line(client_sock, "PONG")) {
                    break;
                }
                continue;
            }

            if (!send_line(client_sock, "ECHO: " + line)) {
                break;
            }
        }
    }

    closesocket(client_sock);
}

int main() {
    TcpSocket::WinsockGuard winsock;
    if (!winsock.ok()) {
        std::cerr << winsock.last_error().message << std::endl;
        return 1;
    }

    SOCKET listen_sock = create_listener(PORT, BACKLOG);
    if (listen_sock == INVALID_SOCKET) {
        return 1;
    }

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

        std::thread(handle_client, client_sock, client_addr).detach();
    }

    closesocket(listen_sock);
    return 0;
}
