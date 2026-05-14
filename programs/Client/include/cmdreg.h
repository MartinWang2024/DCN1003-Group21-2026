#pragma once
#include "SocketHandler.h"
#include "error.h"
#include "database.h"
#include <cstdint>
#include <string>
#include <vector>

namespace dcn_client {

struct ClientResponse {
    uint32_t cmd_type = 0;
    std::string text;
    std::vector<dcn_database::Course> courses;
    bool is_error() const;
    bool is_data() const;
};

Error::ErrorInfo client_login(TcpSocket::SocketHandler& sh, const std::string& user, const std::string& pass, ClientResponse& out);
Error::ErrorInfo client_logout(TcpSocket::SocketHandler& sh, ClientResponse& out);
Error::ErrorInfo client_query_code(TcpSocket::SocketHandler& sh, const std::string& code, ClientResponse& out);
Error::ErrorInfo client_query_instructor(TcpSocket::SocketHandler& sh, const std::string& name, ClientResponse& out);
Error::ErrorInfo client_query_semester(TcpSocket::SocketHandler& sh, const std::string& sem, ClientResponse& out);
Error::ErrorInfo client_view_all(TcpSocket::SocketHandler& sh, ClientResponse& out);
Error::ErrorInfo client_view_all_page(TcpSocket::SocketHandler& sh, int offset, int limit, ClientResponse& out);
Error::ErrorInfo client_add(TcpSocket::SocketHandler& sh, const dcn_database::Course& c, ClientResponse& out);
Error::ErrorInfo client_update(TcpSocket::SocketHandler& sh, const dcn_database::Course& c, ClientResponse& out);
Error::ErrorInfo client_delete(TcpSocket::SocketHandler& sh, const std::string& code, const std::string& section, ClientResponse& out);

}
