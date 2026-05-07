#pragma once

#include <winsock2.h>
#include <atomic>

constexpr const char* SERVER_IP = "127.0.0.1";
constexpr int         PORT      = 8888;
constexpr int         BUF_LEN   = 4096;

extern std::atomic<bool> running;

void recv_thread(SOCKET sock);
