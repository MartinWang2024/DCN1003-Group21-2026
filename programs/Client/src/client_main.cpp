#include "client_main.h"
#include "cmdreg.h"
#include "connect_to.h"
#include "winsock_guard.h"
#include "openssl.h"
#include "log.h"

#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using TcpSocket::SocketHandler;
using dcn_client::ClientResponse;

namespace {

constexpr char TAB = '\t';

std::vector<std::string> tsv_split(const std::string& s)
{
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\r') continue;
        if (c == TAB) { out.push_back(std::move(cur)); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(std::move(cur));
    return out;
}

std::string join_courses_tsv(const std::vector<dcn_database::Course>& cs)
{
    std::ostringstream os;
    os << cs.size();
    for (const auto& c : cs) {
        os << TAB << c.code << TAB << c.title << TAB << c.section
           << TAB << c.instructor << TAB << c.day << TAB << c.duration
           << TAB << c.semester << TAB << c.classroom;
    }
    return os.str();
}


class BridgeSession {
public:
    std::unique_ptr<SocketHandler> sock;

    void emit_ok(const std::string& msg) {
        std::cout << "OK\t" << msg << "\n";
        std::cout.flush();
    }
    void emit_err(const std::string& msg) {
        std::cout << "ERR\t" << msg << "\n";
        std::cout.flush();
    }
    void emit_data(const std::vector<dcn_database::Course>& cs) {
        std::cout << "DATA\t" << join_courses_tsv(cs) << "\n";
        std::cout.flush();
    }

    void emit_resp(const ClientResponse& r) {
        if (r.is_error()) emit_err(r.text);
        else if (r.is_data()) emit_data(r.courses);
        else emit_ok(r.text);
    }

    bool require_connected() {
        if (!sock) { emit_err("not connected"); return false; }
        return true;
    }

    void handle_connect(const std::vector<std::string>& f) {
        if (f.size() < 3) { emit_err("CONNECT requires <host> <port>"); return; }
        int port = std::atoi(f[2].c_str());
        Error::ErrorInfo e;
        auto s = connect_to(f[1], port, e);
        if (e.e != Error::SUCCESS) { emit_err(e.message); return; }
        sock = std::make_unique<SocketHandler>(std::move(s));
        emit_ok("connected");
    }

    void handle_disconnect() {
        sock.reset();
        emit_ok("disconnected");
    }

    void handle_login(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 3) { emit_err("LOGIN requires <user> <pass>"); return; }
        ClientResponse r;
        auto e = dcn_client::client_login(*sock, f[1], f[2], r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_logout() {
        if (!require_connected()) return;
        ClientResponse r;
        auto e = dcn_client::client_logout(*sock, r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_query_code(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 2) { emit_err("QUERY_CODE requires <code>"); return; }
        ClientResponse r;
        auto e = dcn_client::client_query_code(*sock, f[1], r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_query_instructor(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 2) { emit_err("QUERY_INSTRUCTOR requires <name>"); return; }
        ClientResponse r;
        auto e = dcn_client::client_query_instructor(*sock, f[1], r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_query_semester(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 2) { emit_err("QUERY_SEMESTER requires <sem>"); return; }
        ClientResponse r;
        auto e = dcn_client::client_query_semester(*sock, f[1], r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_view_all() {
        if (!require_connected()) return;
        ClientResponse r;
        auto e = dcn_client::client_view_all(*sock, r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_view_all_page(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 3) { emit_err("VIEW_ALL_PAGE requires <offset> <limit>"); return; }
        int offset = std::atoi(f[1].c_str());
        int limit  = std::atoi(f[2].c_str());
        ClientResponse r;
        auto e = dcn_client::client_view_all_page(*sock, offset, limit, r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_add(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 9) { emit_err("ADD requires 8 fields"); return; }
        dcn_database::Course c{f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8]};
        ClientResponse r;
        auto e = dcn_client::client_add(*sock, c, r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_update(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 9) { emit_err("UPDATE requires 8 fields"); return; }
        dcn_database::Course c{f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8]};
        ClientResponse r;
        auto e = dcn_client::client_update(*sock, c, r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }

    void handle_delete(const std::vector<std::string>& f) {
        if (!require_connected()) return;
        if (f.size() < 3) { emit_err("DELETE requires <code> <section>"); return; }
        ClientResponse r;
        auto e = dcn_client::client_delete(*sock, f[1], f[2], r);
        if (e.e != Error::SUCCESS) { emit_err(e.message); sock.reset(); return; }
        emit_resp(r);
    }
};

int run_bridge()
{
    std::cout << "READY\n";
    std::cout.flush();

    BridgeSession sess;
    std::string line;
    while (std::getline(std::cin, line)) {
        auto f = tsv_split(line);
        if (f.empty() || f[0].empty()) continue;
        const std::string& verb = f[0];

        if      (verb == "CONNECT")           sess.handle_connect(f);
        else if (verb == "DISCONNECT")        sess.handle_disconnect();
        else if (verb == "LOGIN")             sess.handle_login(f);
        else if (verb == "LOGOUT")            sess.handle_logout();
        else if (verb == "QUERY_CODE")        sess.handle_query_code(f);
        else if (verb == "QUERY_INSTRUCTOR")  sess.handle_query_instructor(f);
        else if (verb == "QUERY_SEMESTER")    sess.handle_query_semester(f);
        else if (verb == "VIEW_ALL")          sess.handle_view_all();
        else if (verb == "VIEW_ALL_PAGE")     sess.handle_view_all_page(f);
        else if (verb == "ADD")               sess.handle_add(f);
        else if (verb == "UPDATE")            sess.handle_update(f);
        else if (verb == "DELETE")            sess.handle_delete(f);
        else if (verb == "QUIT")              break;
        else                                  sess.emit_err("unknown verb: " + verb);
    }
    return 0;
}


void print_repl_help()
{
    std::cout << "Commands:\n"
              << "  connect <host> <port>\n"
              << "  disconnect\n"
              << "  login <user> <pass>\n"
              << "  logout\n"
              << "  query_code <code>\n"
              << "  query_instructor <name>\n"
              << "  query_semester <sem>\n"
              << "  view_all\n"
              << "  add <code> <title> <section> <instructor> <day> <duration> <semester> <classroom>\n"
              << "  update <same as add>\n"
              << "  delete <code> <section>\n"
              << "  help / quit\n";
}

std::vector<std::string> repl_split(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream is(s);
    std::string tok;
    while (is >> tok) out.push_back(tok);
    return out;
}

void print_resp(const ClientResponse& r)
{
    if (r.is_error()) {
        std::cout << "  [ERR] " << r.text << "\n";
    } else if (r.is_data()) {
        std::cout << "  [DATA] " << r.courses.size() << " course(s):\n";
        for (const auto& c : r.courses) {
            std::cout << "    " << c.code << " " << c.title
                      << " (sec=" << c.section << ") "
                      << c.instructor << " " << c.day
                      << " " << c.duration << "min "
                      << "[" << c.semester << "] @ " << c.classroom << "\n";
        }
    } else {
        std::cout << "  [OK] " << r.text << "\n";
    }
}

int run_repl()
{
    std::cout << "DCN1003 client REPL. Type 'help' for commands.\n";
    std::unique_ptr<SocketHandler> sock;
    std::string line;
    while (std::cout << "> " << std::flush, std::getline(std::cin, line)) {
        auto t = repl_split(line);
        if (t.empty()) continue;
        const std::string& v = t[0];

        if (v == "help") { print_repl_help(); continue; }
        if (v == "quit" || v == "exit") break;

        if (v == "connect") {
            if (t.size() < 3) { std::cout << "  usage: connect <host> <port>\n"; continue; }
            Error::ErrorInfo e;
            auto s = connect_to(t[1], std::atoi(t[2].c_str()), e);
            if (e.e != Error::SUCCESS) { std::cout << "  [ERR] " << e.message << "\n"; continue; }
            sock = std::make_unique<SocketHandler>(std::move(s));
            std::cout << "  [OK] connected\n";
            continue;
        }
        if (v == "disconnect") { sock.reset(); std::cout << "  [OK] disconnected\n"; continue; }
        if (!sock) { std::cout << "  [ERR] not connected\n"; continue; }

        ClientResponse r;
        Error::ErrorInfo e;
        if (v == "login" && t.size() >= 3)              e = dcn_client::client_login(*sock, t[1], t[2], r);
        else if (v == "logout")                          e = dcn_client::client_logout(*sock, r);
        else if (v == "query_code" && t.size() >= 2)     e = dcn_client::client_query_code(*sock, t[1], r);
        else if (v == "query_instructor" && t.size()>=2) e = dcn_client::client_query_instructor(*sock, t[1], r);
        else if (v == "query_semester" && t.size() >= 2) e = dcn_client::client_query_semester(*sock, t[1], r);
        else if (v == "view_all")                        e = dcn_client::client_view_all(*sock, r);
        else if (v == "add" && t.size() >= 9) {
            dcn_database::Course c{t[1],t[2],t[3],t[4],t[5],t[6],t[7],t[8]};
            e = dcn_client::client_add(*sock, c, r);
        }
        else if (v == "update" && t.size() >= 9) {
            dcn_database::Course c{t[1],t[2],t[3],t[4],t[5],t[6],t[7],t[8]};
            e = dcn_client::client_update(*sock, c, r);
        }
        else if (v == "delete" && t.size() >= 3)         e = dcn_client::client_delete(*sock, t[1], t[2], r);
        else { std::cout << "  [ERR] unknown command or wrong arg count\n"; continue; }

        if (e.e != Error::SUCCESS) {
            std::cout << "  [ERR] " << e.message << "\n";
            sock.reset();
        } else {
            print_resp(r);
        }
    }
    return 0;
}

}

int main(int argc, char** argv)
{
    TcpSocket::WinsockGuard winsock;
    if (!winsock.ok()) {
        std::cerr << winsock.last_error().message << std::endl;
        return 1;
    }
    if (openssl::readAppKey(openssl::key, "app.key").e != Error::SUCCESS) {
        std::cerr << "Failed to load app.key from working directory" << std::endl;
        return 1;
    }

    bool bridge_mode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--bridge") bridge_mode = true;
    }
    return bridge_mode ? run_bridge() : run_repl();
}
