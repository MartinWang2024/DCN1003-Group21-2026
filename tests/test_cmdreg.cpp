#include "cmdreg.h"
#include "test.h"
#include "database.h"
#include "message.pb.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Protocal::Dispatch;
using namespace Protocal;

namespace {

constexpr char FIELD_SEP = '\x1F';
constexpr char RECORD_SEP = '\x1E';

// Test fixture: 内存数据库 + 预置数据 + 默认会话
struct Fixture
{
    dcn_database::CourseRepository courses;
    dcn_database::AdministratorRepository admins;
    Session_t session;

    Fixture()
    {
        if (!courses.open(":memory:")) throw std::runtime_error("open courses failed");
        if (!admins.open(":memory:"))  throw std::runtime_error("open admins failed");
        if (!courses.initialize_schema()) throw std::runtime_error("init courses schema failed");
        if (!admins.initialize_schema())  throw std::runtime_error("init admins schema failed");

        admins.insert_or_replace({"alice", "secret123"});

        courses.insert_or_replace({"CS101", "Intro to CS",   "A", "Dr. Smith", "Mon", "09:00-10:30", "2026S1", "Room 1"});
        courses.insert_or_replace({"CS101", "Intro to CS",   "B", "Dr. Smith", "Tue", "09:00-10:30", "2026S1", "Room 2"});
        courses.insert_or_replace({"MA200", "Linear Algebra","A", "Prof. Lee", "Wed", "10:00-11:30", "2026S1", "Room 3"});
    }

    ReqContext_t make_ctx(const MsgBody& body)
    {
        return ReqContext_t{body, session, courses, admins};
    }
};

// 构造一个携带 cmd_type 和若干字符串字段的 MsgBody
MsgBody make_body(uint32_t cmd, std::initializer_list<std::string> fields = {})
{
    MsgBody body;
    body.set_cmd_type(cmd);
    body.set_req_id(1);
    body.set_timestamp(0);
    auto* payload = body.mutable_payload();
    for (const auto& f : fields) payload->add_json(f);
    return body;
}

// 计算 serialize_courses 输出中的记录数: count<RS>...<RS>...
size_t parse_record_count(const std::string& s)
{
    size_t pos = s.find(RECORD_SEP);
    if (pos == std::string::npos) return std::stoul(s);
    return std::stoul(s.substr(0, pos));
}

}  // namespace


// ========== handle_login ==========

TEST(test_login_missing_field)
{
    Fixture f;
    auto body = make_body(Protocal::CMD_LOGIN_REQ, {"alice"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_login(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
    REQUIRE(f.session.role == STUDENT);
}

TEST(test_login_empty_credentials)
{
    Fixture f;
    auto body = make_body(CMD_LOGIN_REQ, {"", ""});
    auto ctx = f.make_ctx(body);
    auto resp = handle_login(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
    REQUIRE(f.session.role == STUDENT);
}

TEST(test_login_wrong_password)
{
    Fixture f;
    auto body = make_body(CMD_LOGIN_REQ, {"alice", "wrong"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_login(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
    REQUIRE(f.session.role == STUDENT);
}

TEST(test_login_success_promotes_session)
{
    Fixture f;
    auto body = make_body(CMD_LOGIN_REQ, {"alice", "secret123"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_login(ctx);
    REQUIRE(resp.cmd_type == CMD_LOGIN_RESP);
    REQUIRE(resp.payload == "OK");
    REQUIRE(f.session.role == ADMIN);
    REQUIRE(f.session.name == "alice");
}

// ========== handle_logout ==========

TEST(test_logout_clears_session)
{
    Fixture f;
    f.session.role = ADMIN;
    f.session.name = "alice";

    auto body = make_body(CMD_LOGOUT_REQ);
    auto ctx = f.make_ctx(body);
    auto resp = handle_logout(ctx);

    REQUIRE(resp.cmd_type == CMD_OK);
    REQUIRE(f.session.role == STUDENT);
    REQUIRE(f.session.name.empty());
}

// ========== handle_query_code ==========

TEST(test_query_code_empty)
{
    Fixture f;
    auto body = make_body(CMD_QUERY_BY_CODE_REQ, {""});
    auto ctx = f.make_ctx(body);
    auto resp = handle_query_code(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

TEST(test_query_code_hit)
{
    Fixture f;
    auto body = make_body(CMD_QUERY_BY_CODE_REQ, {"CS101"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_query_code(ctx);
    REQUIRE(resp.cmd_type == CMD_QUERY_RESP);
    REQUIRE(parse_record_count(resp.payload) == 2);
}

TEST(test_query_code_miss)
{
    Fixture f;
    auto body = make_body(CMD_QUERY_BY_CODE_REQ, {"NONEXIST"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_query_code(ctx);
    REQUIRE(resp.cmd_type == CMD_QUERY_RESP);
    REQUIRE(resp.payload == "0");
}

// ========== handle_query_instructor ==========

TEST(test_query_instructor_hit)
{
    Fixture f;
    auto body = make_body(CMD_QUERY_BY_INSTRUCTOR_REQ, {"Dr. Smith"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_query_instructor(ctx);
    REQUIRE(resp.cmd_type == CMD_QUERY_RESP);
    REQUIRE(parse_record_count(resp.payload) == 2);
}

TEST(test_query_instructor_empty)
{
    Fixture f;
    auto body = make_body(CMD_QUERY_BY_INSTRUCTOR_REQ, {""});
    auto ctx = f.make_ctx(body);
    auto resp = handle_query_instructor(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

// ========== handle_query_semester (stub) ==========

TEST(test_query_semester_returns_error)
{
    Fixture f;
    auto body = make_body(CMD_QUERY_BY_SEMESTER_REQ, {"2026-Spring"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_query_semester(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

// ========== handle_add ==========

TEST(test_add_missing_fields)
{
    Fixture f;
    auto body = make_body(CMD_ADD_REQ, {"PH100", "Physics"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_add(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

TEST(test_add_empty_code)
{
    Fixture f;
    auto body = make_body(CMD_ADD_REQ, {"", "Physics", "A", "Prof. Yang", "Fri", "14:00-15:30", "R5"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_add(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

TEST(test_add_success)
{
    Fixture f;
    auto body = make_body(CMD_ADD_REQ,
        {"PH100", "Physics", "A", "Prof. Yang", "Fri", "14:00-15:30", "R5"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_add(ctx);
    REQUIRE(resp.cmd_type == CMD_ADMIN_RESP);
    REQUIRE(resp.payload == "OK");

    auto found = f.courses.search_by_course_code("PH100");
    REQUIRE(found.size() == 1);
    REQUIRE(found[0].instructor == "Prof. Yang");
}

// ========== handle_update ==========

TEST(test_update_existing_course)
{
    Fixture f;
    auto body = make_body(CMD_UPDATE_REQ,
        {"CS101", "Intro to CS v2", "A", "Dr. Brown", "Mon", "09:00-10:30", "Room 99"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_update(ctx);
    REQUIRE(resp.cmd_type == CMD_ADMIN_RESP);

    auto found = f.courses.search_by_course_code("CS101");
    bool seen_a = false;
    for (const auto& c : found) {
        if (c.section == "A") {
            seen_a = true;
            REQUIRE(c.instructor == "Dr. Brown");
            REQUIRE(c.classroom == "Room 99");
        }
    }
    REQUIRE(seen_a);
}

TEST(test_update_missing_fields)
{
    Fixture f;
    auto body = make_body(CMD_UPDATE_REQ, {"CS101"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_update(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

// ========== handle_delete ==========

TEST(test_delete_existing)
{
    Fixture f;
    auto body = make_body(CMD_DELETE_REQ, {"CS101", "A"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_delete(ctx);
    REQUIRE(resp.cmd_type == CMD_ADMIN_RESP);

    auto remaining = f.courses.search_by_course_code("CS101");
    REQUIRE(remaining.size() == 1);
    REQUIRE(remaining[0].section == "B");
}

TEST(test_delete_missing_fields)
{
    Fixture f;
    auto body = make_body(CMD_DELETE_REQ, {"CS101"});
    auto ctx = f.make_ctx(body);
    auto resp = handle_delete(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

TEST(test_delete_empty_code)
{
    Fixture f;
    auto body = make_body(CMD_DELETE_REQ, {"", ""});
    auto ctx = f.make_ctx(body);
    auto resp = handle_delete(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}

// ========== Dispatcher 路由 + 鉴权 ==========

TEST(test_dispatcher_routes_login)
{
    Fixture f;
    Dispatcher d;
    register_all_server(d);

    auto body = make_body(CMD_LOGIN_REQ, {"alice", "secret123"});
    auto ctx = f.make_ctx(body);
    auto resp = d.dispatch(ctx);
    REQUIRE(resp.cmd_type == CMD_LOGIN_RESP);
    REQUIRE(f.session.role == ADMIN);
}

TEST(test_dispatcher_blocks_unauthorized_admin_cmd)
{
    Fixture f;
    Dispatcher d;
    register_all_server(d);

    auto body = make_body(CMD_DELETE_REQ, {"CS101", "A"});
    auto ctx = f.make_ctx(body);
    auto resp = d.dispatch(ctx);
    REQUIRE(resp.cmd_type == CMD_PERMISSION_ERR);

    auto remaining = f.courses.search_by_course_code("CS101");
    REQUIRE(remaining.size() == 2);
}

TEST(test_dispatcher_unknown_command)
{
    Fixture f;
    Dispatcher d;
    register_all_server(d);

    auto body = make_body(0xDEADBEEF);
    auto ctx = f.make_ctx(body);
    auto resp = d.dispatch(ctx);
    REQUIRE(resp.cmd_type == CMD_ERROR);
}


int main()
{
    std::cout << "Running cmdreg tests...\n";

    RUN(test_login_missing_field);
    RUN(test_login_empty_credentials);
    RUN(test_login_wrong_password);
    RUN(test_login_success_promotes_session);

    RUN(test_logout_clears_session);

    RUN(test_query_code_empty);
    RUN(test_query_code_hit);
    RUN(test_query_code_miss);

    RUN(test_query_instructor_hit);
    RUN(test_query_instructor_empty);

    RUN(test_query_semester_returns_error);

    RUN(test_add_missing_fields);
    RUN(test_add_empty_code);
    RUN(test_add_success);

    RUN(test_update_existing_course);
    RUN(test_update_missing_fields);

    RUN(test_delete_existing);
    RUN(test_delete_missing_fields);
    RUN(test_delete_empty_code);

    RUN(test_dispatcher_routes_login);
    RUN(test_dispatcher_blocks_unauthorized_admin_cmd);
    RUN(test_dispatcher_unknown_command);

    std::cout << "\nPassed: " << s_passed << ", Failed: " << s_failed << "\n";
    return s_failed == 0 ? 0 : 1;
}
