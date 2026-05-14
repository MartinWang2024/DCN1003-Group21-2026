#pragma once
#include "SocketHandler.h"
#include <string>

// 客户端连接工厂: 创建 TCP socket, 解析地址, connect 到 server
// 成功返回 SocketHandler (移动语义), 失败返回 INVALID_SOCKET 包装的 SocketHandler
// 与 server 端 listener.cpp 对称
TcpSocket::SocketHandler connect_to(const std::string& host, int port, Error::ErrorInfo& out_err);
