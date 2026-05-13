#pragma once

#include <winsock2.h>
#include <string>

constexpr int PORT    = 9001;
constexpr int BACKLOG = 10;
constexpr int BUF_LEN = 4096;

// 数据库文件路径（相对于可执行文件的工作目录，
// 启动 Service 时请在项目根目录下运行，或改为绝对路径）
constexpr const char* DB_COURSES_PATH = "data/courses.db";
constexpr const char* DB_ADMINS_PATH  = "data/admins.db";

bool send_line(SOCKET sock, const std::string& text);

void handle_client(SOCKET client_sock, sockaddr_in client_addr);
