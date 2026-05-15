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

    // Session state
    struct Session_t
    {
        role_t role = STUDENT;
        std::string name;
    };

    // Request context
    struct ReqContext_t
    {
        const MsgBody& body;
        Session_t& session;
        dcn_database::CourseRepository& course_repo;
        dcn_database::AdministratorRepository& admin_repo;
    };

    // Response payload
    #pragma pack(push, 1)
    struct Response_t
    {
        cmdtype_t cmd_type;
        std::string payload;
    };
    #pragma pack(pop)

    // Handler signature
    using HandlerFn = std::function<Response_t(const ReqContext_t&)>;

    class Dispatcher {
    public:
        /**
         * Register a command handler.
         * @param cmd      Command code
         * @param required Minimum role required
         * @param fn       Handler function
         */
        Error::ErrorInfo register_handler(cmdtype_t cmd, role_t required, HandlerFn fn);

        /**
         * Dispatch an incoming request to the registered handler.
         * Performs centralized role-based access control before invoking the handler.
         * Never throws — exceptions are caught and turned into CMD_SERVER_ERROR.
         * @param ctx  Request context with body, session, and DB repos
         * @return     Response with cmd_type and serialized payload
         */
        Response_t dispatch(const ReqContext_t& ctx);

        /**
         * Send a response on the server side (S2C direction).
         * @param sh   Socket handle
         * @param resp Response to send
         * @return     Error code
         */
        static Error::ErrorInfo send_response_s(TcpSocket::SocketHandler& sh, const Response_t& resp);

        /**
         * Send a response on the client side (C2S direction).
         * @param sh   Socket handle
         * @param resp Response to send
         * @return     Error code
         */
        static Error::ErrorInfo send_response_c(TcpSocket::SocketHandler& sh, const Response_t& resp);

    private:
        // (handler, required_role) pair
        struct Entry_t {
            role_t required;
            HandlerFn fn;
        };
        std::unordered_map<uint32_t, Entry_t> table_;  // command registry
    };

}
