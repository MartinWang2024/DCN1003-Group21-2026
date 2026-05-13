#pragma once
#include <string>

#include "database.h"
#include "message.pb.h"
#include "cmd_type.h"
#include "error.h"
#include "log.h"
#include "functional"
#include "SocketHandler.h"
#include "unordered_map"


namespace Protocal::Dispatch
{
    enum role_t
    {
        STUDENT = 0,
        ADMIN = 1,
    };

    // 会话信息
    struct Session_t
    {
        role_t role = STUDENT;
        std::string name;
    };

    // 请求上下文
    struct ReqContext_t
    {
        const MsgBody& body;
        Session_t& session;
        dcn_database::CourseRepository& course_repo;
        dcn_database::AdministratorRepository& admin_repo;
    };

    // 响应数据结构
    struct Response_t
    {
        uint32_t cmd_type;
        std::string payload;
    };

    // 函数模板
    using HandlerFn = std::function<Response_t(const ReqContext_t&)>;

    class Dispatcher {
    public:
        /**
         * 注册命令
         * @param cmd 命令码
         * @param required 权限需求
         * @param fn 命令对应的处理函数
         */
        Error::ErrorInfo register_handler(cmdtype_t cmd, role_t required, HandlerFn fn);

        /**
         * 命令 handler 调度器
         * @param ctx 请求上下文，包含请求数据、会话信息和数据库访问接口
         * @return 响应数据，包含响应命令码和响应负载
         */
        Response_t dispatch(const ReqContext_t& ctx);

        /**
         * 操作传输层函数发送响应数据-服务端函数
         * @param sh socket handler 句柄，包含发送接口和客户端信息
         * @param resp 来自会话层的响应数据，包含响应命令码和响应负载
         * @return 错误码
         */
        static Error::ErrorInfo send_response_s(TcpSocket::SocketHandler& sh, const Response_t& resp);

        /**
         * 操作传输层函数发送响应数据-客户端函数
         * @param sh socket handler 句柄，包含发送接口和客户端信息
         * @param resp 来自会话层的响应数据，包含响应命令码和响应负载
         * @return 错误码
         */
        static Error::ErrorInfo send_response_c(TcpSocket::SocketHandler& sh, const Response_t& resp);

    private:
        // 处理函数与其对应权限的组合结构体
        struct Entry_t {
            role_t required;
            HandlerFn fn;
        };
        std::unordered_map<uint32_t, Entry_t> table_;  // 命令注册表
    };

}
