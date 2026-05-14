#pragma once
#include <winsock2.h>

// 创建并启动一个 IPv4 TCP 监听 socket
// 内部完成: socket -> SO_REUSEADDR -> bind(INADDR_ANY:port) -> listen(backlog)
// 失败时返回 INVALID_SOCKET 并打印错误日志, 调用方仅需判 INVALID_SOCKET
// 调用方负责: 在不再需要时 closesocket 返回值
SOCKET create_listener(int port, int backlog);
