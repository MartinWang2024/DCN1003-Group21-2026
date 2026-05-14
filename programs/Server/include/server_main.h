#pragma once

#include "CmdHandler.h"
#include "SocketHandler.h"
#include "database.h"

constexpr int PORT    = 9001;
constexpr int BACKLOG = 10;

// 数据库文件路径（相对于可执行文件的工作目录，
// 启动 Service 时请在项目根目录下运行，或改为绝对路径）
constexpr const char* DB_COURSES_PATH = "data/courses.db";
constexpr const char* DB_ADMINS_PATH  = "data/admins.db";

// 单个客户端连接的处理线程
// sh 通过移动接管 socket 所有权; courses/admins/dispatcher 全程引用 main 的实例
void handle_client(TcpSocket::SocketHandler sh,
                   dcn_database::CourseRepository& courses,
                   dcn_database::AdministratorRepository& admins,
                   Protocal::Dispatch::Dispatcher& dispatcher);
