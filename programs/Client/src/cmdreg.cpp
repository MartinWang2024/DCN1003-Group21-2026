#include "cmdreg.h"

#include "cmd_type.h"
#include "protocol.h"
#include "message.pb.h"

#include <sstream>

namespace dcn_client {

namespace {

constexpr char FIELD_SEP  = '\x1F';
constexpr char RECORD_SEP = '\x1E';

std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(std::move(cur)); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(std::move(cur));
    return out;
}

std::vector<dcn_database::Course> parse_courses(const std::string& payload)
{
    std::vector<dcn_database::Course> result;
    if (payload.empty()) return result;
    auto records = split(payload, RECORD_SEP);
    for (size_t i = 1; i < records.size(); ++i) {
        auto f = split(records[i], FIELD_SEP);
        if (f.size() < 7) continue;
        result.push_back({f[0], f[1], f[2], f[3], f[4], f[5], f[6]});
    }
    return result;
}

Error::ErrorInfo send_and_recv(
    TcpSocket::SocketHandler& sh,
    const std::vector<std::string>& fields,
    uint32_t cmd,
    ClientResponse& out)
{
    Error::ErrorInfo err = Protocal::Package_send(sh, fields, cmd);
    if (err.e != Error::SUCCESS) return err;

    MsgBody resp;
    err = Protocal::Package_receive(sh, resp);
    if (err.e != Error::SUCCESS) return err;

    out.cmd_type = resp.cmd_type();
    const auto& p = resp.payload();
    out.text = (p.json_size() > 0) ? p.json(0) : std::string{};

    if (out.cmd_type == Protocal::CMD_QUERY_RESP) {
        out.courses = parse_courses(out.text);
    }
    return err;
}

}

bool ClientResponse::is_error() const {
    return cmd_type == Protocal::CMD_ERROR
        || cmd_type == Protocal::CMD_PERMISSION_ERR
        || cmd_type == Protocal::CMD_SERVER_ERROR;
}

bool ClientResponse::is_data() const {
    return cmd_type == Protocal::CMD_QUERY_RESP;
}

Error::ErrorInfo client_login(TcpSocket::SocketHandler& sh, const std::string& user, const std::string& pass, ClientResponse& out)
{ return send_and_recv(sh, {user, pass}, Protocal::CMD_LOGIN_REQ, out); }

Error::ErrorInfo client_logout(TcpSocket::SocketHandler& sh, ClientResponse& out)
{ return send_and_recv(sh, {}, Protocal::CMD_LOGOUT_REQ, out); }

Error::ErrorInfo client_query_code(TcpSocket::SocketHandler& sh, const std::string& code, ClientResponse& out)
{ return send_and_recv(sh, {code}, Protocal::CMD_QUERY_BY_CODE_REQ, out); }

Error::ErrorInfo client_query_instructor(TcpSocket::SocketHandler& sh, const std::string& name, ClientResponse& out)
{ return send_and_recv(sh, {name}, Protocal::CMD_QUERY_BY_INSTRUCTOR_REQ, out); }

Error::ErrorInfo client_query_semester(TcpSocket::SocketHandler& sh, const std::string& sem, ClientResponse& out)
{ return send_and_recv(sh, {sem}, Protocal::CMD_QUERY_BY_SEMESTER_REQ, out); }

Error::ErrorInfo client_view_all(TcpSocket::SocketHandler& sh, ClientResponse& out)
{ return send_and_recv(sh, {}, Protocal::CMD_VIEW_ALL_REQ, out); }

Error::ErrorInfo client_add(TcpSocket::SocketHandler& sh, const dcn_database::Course& c, ClientResponse& out)
{ return send_and_recv(sh, {c.code, c.title, c.section, c.instructor, c.day, c.duration, c.classroom}, Protocal::CMD_ADD_REQ, out); }

Error::ErrorInfo client_update(TcpSocket::SocketHandler& sh, const dcn_database::Course& c, ClientResponse& out)
{ return send_and_recv(sh, {c.code, c.title, c.section, c.instructor, c.day, c.duration, c.classroom}, Protocal::CMD_UPDATE_REQ, out); }

Error::ErrorInfo client_delete(TcpSocket::SocketHandler& sh, const std::string& code, const std::string& section, ClientResponse& out)
{ return send_and_recv(sh, {code, section}, Protocal::CMD_DELETE_REQ, out); }

}
