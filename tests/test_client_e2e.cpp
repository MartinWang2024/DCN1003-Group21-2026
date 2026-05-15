// Tests the client API: connect_to + the 8 client_* functions.
// Spins up an in-process loopback server (real socket + Dispatcher + in-memory DB);
// the client connects via connect_to, exercises all 8 calls and verifies response parsing.
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "test.h"

#include "CmdHandler.h"
#include "SocketHandler.h"
#include "cmd_type.h"
#include "database.h"
#include "openssl.h"
#include "protocol.h"

#include "cmdreg.h"
#include "connect_to.h"

using namespace Protocal;
using namespace Protocal::Dispatch;

extern void register_all_server(Dispatcher& dispatcher);

#pragma comment(lib, "Ws2_32.lib")

struct WinsockGuard {
    WinsockGuard() {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
            throw std::runtime_error("WSAStartup failed");
    }
    ~WinsockGuard() { WSACleanup(); }
};


// Loopback server: bind to a random port, single-thread accept of a single client, run the session via Dispatcher until the client disconnects.
class LoopbackServer
{
public:
    LoopbackServer(dcn_database::CourseRepository& courses,
                   dcn_database::AdministratorRepository& admins)
        : courses_(courses), admins_(admins)
    {
        register_all_server(dispatcher_);
        listener_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listener_ == INVALID_SOCKET) throw std::runtime_error("server socket failed");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        if (bind(listener_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
            throw std::runtime_error("server bind failed");
        if (listen(listener_, 1) == SOCKET_ERROR)
            throw std::runtime_error("server listen failed");

        int alen = sizeof(addr);
        getsockname(listener_, reinterpret_cast<sockaddr*>(&addr), &alen);
        port_ = ntohs(addr.sin_port);

        thr_ = std::thread([this]{ run(); });
    }

    ~LoopbackServer() { stop(); }

    int port() const { return port_; }

    void stop()
    {
        if (stopped_.exchange(true)) return;
        if (listener_ != INVALID_SOCKET) {
            closesocket(listener_);
            listener_ = INVALID_SOCKET;
        }
        SOCKET s = srv_sock_.exchange(INVALID_SOCKET);
        if (s != INVALID_SOCKET) {
            shutdown(s, SD_BOTH);
            closesocket(s);
        }
        if (thr_.joinable()) thr_.join();
    }

private:
    void run()
    {
        sockaddr_in peer{};
        int plen = sizeof(peer);
        SOCKET srv = accept(listener_, reinterpret_cast<sockaddr*>(&peer), &plen);
        if (srv == INVALID_SOCKET) return;
        srv_sock_.store(srv);

        TcpSocket::SocketHandler sh(srv, peer);
        Session_t session;

        while (true)
        {
            MsgBody body;
            auto rerr = Package_receive(sh, body);
            if (rerr.e != Error::SUCCESS) break;

            ReqContext_t ctx{body, session, courses_, admins_};
            Response_t resp = dispatcher_.dispatch(ctx);

            auto serr = Dispatcher::send_response_s(sh, resp);
            if (serr.e != Error::SUCCESS) break;
        }
        srv_sock_.store(INVALID_SOCKET);
    }

    dcn_database::CourseRepository& courses_;
    dcn_database::AdministratorRepository& admins_;
    Dispatcher dispatcher_;
    SOCKET listener_ = INVALID_SOCKET;
    std::atomic<SOCKET> srv_sock_{INVALID_SOCKET};
    int port_ = 0;
    std::thread thr_;
    std::atomic<bool> stopped_{false};
};


// Test fixture: in-memory DB + default admin/course; start server and connect_to immediately.
// Server is wrapped in unique_ptr because LoopbackServer holds a thread/atomic and is not movable.
struct E2EFixture
{
    dcn_database::CourseRepository courses;
    dcn_database::AdministratorRepository admins;
    std::unique_ptr<LoopbackServer> server;
    TcpSocket::SocketHandler client;

    E2EFixture()
        : server(init_and_make_server()),
          client(connect_client(server->port()))
    {}

    ~E2EFixture() { if (server) server->stop(); }

    std::unique_ptr<LoopbackServer> init_and_make_server()
    {
        if (!courses.open(":memory:")) throw std::runtime_error("courses open");
        if (!admins.open(":memory:"))  throw std::runtime_error("admins open");
        if (!courses.initialize_schema()) throw std::runtime_error("courses schema");
        if (!admins.initialize_schema())  throw std::runtime_error("admins schema");
        admins.insert_or_replace({"alice", "secret123"});
        courses.insert_or_replace({"CS101", "Intro to CS",   "A", "Dr. Smith", "Mon", "09:00-10:30", "Room 1"});
        courses.insert_or_replace({"CS101", "Intro to CS",   "B", "Dr. Smith", "Tue", "09:00-10:30", "Room 2"});
        courses.insert_or_replace({"MA200", "Linear Algebra","A", "Prof. Lee", "Wed", "10:00-11:30", "Room 3"});
        return std::make_unique<LoopbackServer>(courses, admins);
    }

    static TcpSocket::SocketHandler connect_client(int port)
    {
        Error::ErrorInfo err;
        auto sh = connect_to("127.0.0.1", port, err);
        if (err.e != Error::SUCCESS) throw std::runtime_error("connect_to failed: " + err.message);
        return sh;
    }
};


// ========== connect_to failure paths ==========

TEST(test_connect_to_invalid_ip)
{
    Error::ErrorInfo err;
    auto sh = connect_to("not-an-ip", 9001, err);
    REQUIRE(err.e == Error::SOCKET_ERR);
}

TEST(test_connect_to_unused_port_fails)
{
    Error::ErrorInfo err;
    auto sh = connect_to("127.0.0.1", 1, err);
    REQUIRE(err.e == Error::SOCKET_ERR);
}


// ========== Basic round-trip: login success path ==========

TEST(test_login_success_roundtrip)
{
    E2EFixture f;
    dcn_client::ClientResponse out;
    auto err = dcn_client::client_login(f.client, "alice", "secret123", out);
    REQUIRE(err.e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_LOGIN_RESP);
    REQUIRE(!out.is_error());
    REQUIRE(out.text == "OK");
}

TEST(test_login_wrong_password_returns_error)
{
    E2EFixture f;
    dcn_client::ClientResponse out;
    auto err = dcn_client::client_login(f.client, "alice", "wrong", out);
    REQUIRE(err.e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_ERROR);
    REQUIRE(out.is_error());
}


// ========== Queries: parse_courses deserialization checks ==========

TEST(test_query_code_parses_courses_correctly)
{
    E2EFixture f;
    dcn_client::ClientResponse out;
    auto err = dcn_client::client_query_code(f.client, "CS101", out);
    REQUIRE(err.e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_QUERY_RESP);
    REQUIRE(out.is_data());
    REQUIRE(out.courses.size() == 2);

    bool seen_a = false, seen_b = false;
    for (const auto& c : out.courses) {
        REQUIRE(c.code == "CS101");
        REQUIRE(c.title == "Intro to CS");
        REQUIRE(c.instructor == "Dr. Smith");
        if (c.section == "A") { seen_a = true; REQUIRE(c.classroom == "Room 1"); }
        if (c.section == "B") { seen_b = true; REQUIRE(c.classroom == "Room 2"); }
    }
    REQUIRE(seen_a);
    REQUIRE(seen_b);
}

TEST(test_query_code_miss_returns_zero_courses)
{
    E2EFixture f;
    dcn_client::ClientResponse out;
    auto err = dcn_client::client_query_code(f.client, "ZZ999", out);
    REQUIRE(err.e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_QUERY_RESP);
    REQUIRE(out.courses.empty());
}

TEST(test_query_instructor_roundtrip)
{
    E2EFixture f;
    dcn_client::ClientResponse out;
    auto err = dcn_client::client_query_instructor(f.client, "Prof. Lee", out);
    REQUIRE(err.e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_QUERY_RESP);
    REQUIRE(out.courses.size() == 1);
    REQUIRE(out.courses[0].code == "MA200");
}

TEST(test_query_semester_returns_error)
{
    E2EFixture f;
    dcn_client::ClientResponse out;
    auto err = dcn_client::client_query_semester(f.client, "2026-Spring", out);
    REQUIRE(err.e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_ERROR);
    REQUIRE(out.is_error());
}


// ========== Writes: authorization + end-to-end ==========

TEST(test_add_without_login_blocked_by_dispatcher)
{
    E2EFixture f;
    dcn_database::Course c{"PH100", "Physics", "A", "Prof. Yang", "Fri", "14:00-15:30", "R5"};
    dcn_client::ClientResponse out;
    auto err = dcn_client::client_add(f.client, c, out);
    REQUIRE(err.e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_PERMISSION_ERR);
    REQUIRE(out.is_error());

    auto found = f.courses.search_by_course_code("PH100");
    REQUIRE(found.empty());
}

TEST(test_add_after_login_succeeds_and_persists)
{
    E2EFixture f;
    dcn_client::ClientResponse login_out;
    REQUIRE(dcn_client::client_login(f.client, "alice", "secret123", login_out).e == Error::SUCCESS);
    REQUIRE(login_out.cmd_type == CMD_LOGIN_RESP);

    dcn_database::Course c{"PH100", "Physics", "A", "Prof. Yang", "Fri", "14:00-15:30", "R5"};
    dcn_client::ClientResponse out;
    REQUIRE(dcn_client::client_add(f.client, c, out).e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_ADMIN_RESP);
    REQUIRE(out.text == "OK");

    auto found = f.courses.search_by_course_code("PH100");
    REQUIRE(found.size() == 1);
    REQUIRE(found[0].instructor == "Prof. Yang");
}

TEST(test_update_after_login_changes_db)
{
    E2EFixture f;
    dcn_client::ClientResponse tmp;
    REQUIRE(dcn_client::client_login(f.client, "alice", "secret123", tmp).e == Error::SUCCESS);

    dcn_database::Course c{"CS101", "Intro to CS v2", "A", "Dr. Brown", "Mon", "09:00-10:30", "Room 99"};
    dcn_client::ClientResponse out;
    REQUIRE(dcn_client::client_update(f.client, c, out).e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_ADMIN_RESP);

    auto found = f.courses.search_by_course_code("CS101");
    bool seen_a = false;
    for (const auto& cc : found)
        if (cc.section == "A") { seen_a = true; REQUIRE(cc.instructor == "Dr. Brown"); REQUIRE(cc.classroom == "Room 99"); }
    REQUIRE(seen_a);
}

TEST(test_delete_after_login_removes_row)
{
    E2EFixture f;
    dcn_client::ClientResponse tmp;
    REQUIRE(dcn_client::client_login(f.client, "alice", "secret123", tmp).e == Error::SUCCESS);

    dcn_client::ClientResponse out;
    REQUIRE(dcn_client::client_delete(f.client, "CS101", "A", out).e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_ADMIN_RESP);

    auto remaining = f.courses.search_by_course_code("CS101");
    REQUIRE(remaining.size() == 1);
    REQUIRE(remaining[0].section == "B");
}

TEST(test_logout_drops_admin_role)
{
    E2EFixture f;
    dcn_client::ClientResponse tmp;
    REQUIRE(dcn_client::client_login(f.client, "alice", "secret123", tmp).e == Error::SUCCESS);
    REQUIRE(dcn_client::client_logout(f.client, tmp).e == Error::SUCCESS);
    REQUIRE(tmp.cmd_type == CMD_OK);

    dcn_client::ClientResponse out;
    REQUIRE(dcn_client::client_delete(f.client, "CS101", "A", out).e == Error::SUCCESS);
    REQUIRE(out.cmd_type == CMD_PERMISSION_ERR);
}


int main()
{
    WinsockGuard guard;

    if (openssl::readAppKey(openssl::key, "app.key").e != Error::SUCCESS) {
        std::cerr << "Failed to load app.key from working directory\n";
        return 2;
    }

    std::cout << "Running client E2E tests...\n";

    RUN(test_connect_to_invalid_ip);
    RUN(test_connect_to_unused_port_fails);

    RUN(test_login_success_roundtrip);
    RUN(test_login_wrong_password_returns_error);

    RUN(test_query_code_parses_courses_correctly);
    RUN(test_query_code_miss_returns_zero_courses);
    RUN(test_query_instructor_roundtrip);
    RUN(test_query_semester_returns_error);

    RUN(test_add_without_login_blocked_by_dispatcher);
    RUN(test_add_after_login_succeeds_and_persists);
    RUN(test_update_after_login_changes_db);
    RUN(test_delete_after_login_removes_row);
    RUN(test_logout_drops_admin_role);

    std::cout << "\nPassed: " << s_passed << ", Failed: " << s_failed << "\n";
    return s_failed == 0 ? 0 : 1;
}
