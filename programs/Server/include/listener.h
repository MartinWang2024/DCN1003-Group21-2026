#pragma once
#include <winsock2.h>

// Create and start an IPv4 TCP listening socket.
// Performs: socket -> SO_REUSEADDR -> bind(INADDR_ANY:port) -> listen(backlog).
// Returns INVALID_SOCKET on failure (with error logged); caller only checks INVALID_SOCKET.
// Caller owns the returned socket and must closesocket() when done.
SOCKET create_listener(int port, int backlog);
