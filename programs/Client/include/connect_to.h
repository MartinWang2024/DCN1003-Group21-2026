#pragma once
#include "SocketHandler.h"
#include <string>

// Client connection factory: create TCP socket, resolve address, connect to server.
// On success returns SocketHandler (move-only); on failure returns a SocketHandler wrapping INVALID_SOCKET.
// Symmetric counterpart of listener.cpp on the server side.
TcpSocket::SocketHandler connect_to(const std::string& host, int port, Error::ErrorInfo& out_err);
