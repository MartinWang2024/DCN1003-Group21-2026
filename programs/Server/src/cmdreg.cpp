#include "cmdreg.h"

#include <sstream>

using namespace Protocal::Dispatch;

namespace {

// 查询结果序列化分隔符: 0x1F=字段间, 0x1E=记录间
// 选用 ASCII 控制字符避免与课程字段（中英文/数字/空格）冲突
constexpr char FIELD_SEP = '\x1F';
constexpr char RECORD_SEP = '\x1E';

// 把 Course 列表序列化为响应 payload
// 格式: <count><RS><c1.code><US><c1.title>...<US><c1.classroom><RS><c2...>
std::string serialize_courses(const std::vector<dcn_database::Course>& courses)
{
    std::ostringstream os;
    os << courses.size();
    for (const auto& c : courses)
    {
        os << RECORD_SEP
           << c.code       << FIELD_SEP
           << c.title      << FIELD_SEP
           << c.section    << FIELD_SEP
           << c.instructor << FIELD_SEP
           << c.day        << FIELD_SEP
           << c.duration   << FIELD_SEP
           << c.classroom;
    }
    return os.str();
}

// 把 payload.json 数组按位置取字段, 缺位返回空串
std::string field_at(const Payload& p, int idx)
{
    return idx < p.json_size() ? p.json(idx) : std::string{};
}

}  // namespace


// 登录处理
// 协议: payload.json[0] = username, payload.json[1] = password
// 成功: session 升级为 ADMIN, 返回 CMD_LOGIN_RESP + "OK"
// 失败: 返回 CMD_ERROR + 错误说明
Response_t handle_login(const ReqContext_t& ctx)
{
    Response_t resp;

    const auto& payload = ctx.body.payload();
    if (payload.json_size() < 2)
    {
        print_log(warn, "handle_login: payload field missing, got %d", payload.json_size());
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Login payload requires username and password";
        return resp;
    }

    const std::string username = payload.json(0);
    const std::string password = payload.json(1);

    if (username.empty() || password.empty())
    {
        print_log(warn, "handle_login: empty username or password");
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Username or password cannot be empty";
        return resp;
    }

    if (!ctx.admin_repo.verify_login(username, password))
    {
        print_log(warn, "handle_login: invalid credentials for user '%s'", username.c_str());
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Invalid username or password";
        return resp;
    }

    ctx.session.role = ADMIN;
    ctx.session.name = username;

    print_log(info, "handle_login: user '%s' logged in as ADMIN", username.c_str());
    resp.cmd_type = Protocal::CMD_LOGIN_RESP;
    resp.payload = "OK";
    return resp;
}

// 登出处理: 把 session 降级回 STUDENT 并清空用户名
Response_t handle_logout(const ReqContext_t& ctx)
{
    Response_t resp;
    print_log(info, "handle_logout: user '%s' logged out", ctx.session.name.c_str());
    ctx.session.role = STUDENT;
    ctx.session.name.clear();
    resp.cmd_type = Protocal::CMD_OK;
    resp.payload = "OK";
    return resp;
}

// 按课程编码查询
// 协议: payload.json[0] = course code
Response_t handle_query_code(const ReqContext_t& ctx)
{
    Response_t resp;
    const std::string code = field_at(ctx.body.payload(), 0);
    if (code.empty())
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Course code required";
        return resp;
    }

    const auto courses = ctx.course_repo.search_by_course_code(code);
    resp.cmd_type = Protocal::CMD_QUERY_RESP;
    resp.payload = serialize_courses(courses);
    print_log(info, "handle_query_code: code='%s', %zu results", code.c_str(), courses.size());
    return resp;
}

// 按教师姓名查询
// 协议: payload.json[0] = instructor name
Response_t handle_query_instructor(const ReqContext_t& ctx)
{
    Response_t resp;
    const std::string instructor = field_at(ctx.body.payload(), 0);
    if (instructor.empty())
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Instructor name required";
        return resp;
    }

    const auto courses = ctx.course_repo.search_by_instructor(instructor);
    resp.cmd_type = Protocal::CMD_QUERY_RESP;
    resp.payload = serialize_courses(courses);
    print_log(info, "handle_query_instructor: name='%s', %zu results",
              instructor.c_str(), courses.size());
    return resp;
}

// 按学期查询
// FIXME: 当前 Course schema 无 semester 字段, CourseRepository 无对应接口
// 暂返回错误, 待数据库扩展后补齐
Response_t handle_query_semester(const ReqContext_t& /*ctx*/)
{
    Response_t resp;
    print_log(warn, "handle_query_semester: not supported (schema missing semester column)");
    resp.cmd_type = Protocal::CMD_ERROR;
    resp.payload = "Query by semester not supported in current schema";
    return resp;
}

Response_t handle_view_all(const ReqContext_t& ctx)
{
    Response_t resp;
    const auto courses = ctx.course_repo.view_all_courses();
    resp.cmd_type = Protocal::CMD_QUERY_RESP;
    resp.payload = serialize_courses(courses);
    print_log(info, "handle_view_all: %zu results", courses.size());
    return resp;
}

// 增加课程 (admin)
// 协议: payload.json[0..6] = code/title/section/instructor/day/duration/classroom
Response_t handle_add(const ReqContext_t& ctx)
{
    Response_t resp;
    const auto& p = ctx.body.payload();
    if (p.json_size() < 7)
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Add requires 7 fields (code/title/section/instructor/day/duration/classroom)";
        return resp;
    }

    dcn_database::Course course{
        p.json(0), p.json(1), p.json(2), p.json(3),
        p.json(4), p.json(5), p.json(6)
    };

    if (course.code.empty() || course.section.empty())
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Course code and section cannot be empty";
        return resp;
    }

    if (!ctx.course_repo.insert_or_replace(course))
    {
        print_log(error, "handle_add: insert failed: %s",
                  ctx.course_repo.last_error().c_str());
        resp.cmd_type = Protocal::CMD_SERVER_ERROR;
        resp.payload = "Failed to insert course: " + ctx.course_repo.last_error();
        return resp;
    }

    print_log(info, "handle_add: added course '%s'/'%s'",
              course.code.c_str(), course.section.c_str());
    resp.cmd_type = Protocal::CMD_ADMIN_RESP;
    resp.payload = "OK";
    return resp;
}

// 更新课程 (admin)
// 协议: payload.json[0..6] = code/title/section/instructor/day/duration/classroom
// (code, section) 用于定位记录, 其余字段为新值
Response_t handle_update(const ReqContext_t& ctx)
{
    Response_t resp;
    const auto& p = ctx.body.payload();
    if (p.json_size() < 7)
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Update requires 7 fields";
        return resp;
    }

    dcn_database::Course course{
        p.json(0), p.json(1), p.json(2), p.json(3),
        p.json(4), p.json(5), p.json(6)
    };

    if (course.code.empty() || course.section.empty())
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Course code and section cannot be empty";
        return resp;
    }

    if (!ctx.course_repo.update(course))
    {
        print_log(error, "handle_update: update failed: %s",
                  ctx.course_repo.last_error().c_str());
        resp.cmd_type = Protocal::CMD_SERVER_ERROR;
        resp.payload = "Failed to update course: " + ctx.course_repo.last_error();
        return resp;
    }

    print_log(info, "handle_update: updated '%s'/'%s'",
              course.code.c_str(), course.section.c_str());
    resp.cmd_type = Protocal::CMD_ADMIN_RESP;
    resp.payload = "OK";
    return resp;
}

// 删除课程 (admin)
// 协议: payload.json[0] = code, payload.json[1] = section
Response_t handle_delete(const ReqContext_t& ctx)
{
    Response_t resp;
    const auto& p = ctx.body.payload();
    if (p.json_size() < 2)
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Delete requires code and section";
        return resp;
    }

    const std::string code = p.json(0);
    const std::string section = p.json(1);
    if (code.empty() || section.empty())
    {
        resp.cmd_type = Protocal::CMD_ERROR;
        resp.payload = "Course code and section cannot be empty";
        return resp;
    }

    if (!ctx.course_repo.delete_course(code, section))
    {
        print_log(error, "handle_delete: delete failed: %s",
                  ctx.course_repo.last_error().c_str());
        resp.cmd_type = Protocal::CMD_SERVER_ERROR;
        resp.payload = "Failed to delete course: " + ctx.course_repo.last_error();
        return resp;
    }

    print_log(info, "handle_delete: deleted '%s'/'%s'", code.c_str(), section.c_str());
    resp.cmd_type = Protocal::CMD_ADMIN_RESP;
    resp.payload = "OK";
    return resp;
}

void register_all_server(Dispatcher& dispatcher)
{
    dispatcher.register_handler(Protocal::CMD_LOGIN_REQ,               STUDENT, handle_login);
    dispatcher.register_handler(Protocal::CMD_LOGOUT_REQ,              STUDENT, handle_logout);
    dispatcher.register_handler(Protocal::CMD_QUERY_BY_CODE_REQ,       STUDENT, handle_query_code);
    dispatcher.register_handler(Protocal::CMD_QUERY_BY_INSTRUCTOR_REQ, STUDENT, handle_query_instructor);
    dispatcher.register_handler(Protocal::CMD_QUERY_BY_SEMESTER_REQ,   STUDENT, handle_query_semester);
    dispatcher.register_handler(Protocal::CMD_VIEW_ALL_REQ,            STUDENT, handle_view_all);
    dispatcher.register_handler(Protocal::CMD_ADD_REQ,                 ADMIN,   handle_add);
    dispatcher.register_handler(Protocal::CMD_UPDATE_REQ,              ADMIN,   handle_update);
    dispatcher.register_handler(Protocal::CMD_DELETE_REQ,              ADMIN,   handle_delete);
    print_log(info, "register_all_server: 9 handlers registered");
}
