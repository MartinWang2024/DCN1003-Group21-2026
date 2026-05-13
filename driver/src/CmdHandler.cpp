#include "CmdHandler.h"

#include <utility>



using namespace Protocal::Dispatch;

Error::ErrorInfo Dispatcher::register_handler(cmdtype_t cmd, role_t required, HandlerFn fn)
{
    Error::ErrorInfo err;

    table_[cmd] = Entry_t{required,std::move(fn)};
    print_log(info, "Registered handler for cmd 0x%X with required role %d", cmd, required);

    return err;
}

Response_t Dispatcher::dispatch(const ReqContext_t& ctx)
{
    Response_t resp;
    const auto it = table_.find(ctx.body.cmd_type());
    if (it == table_.end())
    {
        print_log(warn, "No handler found for cmd 0x%X", ctx.body.cmd_type());
        // 这个函数无论如何都不能崩！！！！
        resp.cmd_type = CMD_ERROR;
        resp.payload = "Unknown command";
        return resp;
    }

    // 权限检查
    const Entry_t& entr = it->second;
    if (ctx.session.role < entr.required)
    {
        print_log(warn, "Insufficient permissions for cmd 0x%X: required role %d, but session role is %d",
                  ctx.body.cmd_type(), entr.required, ctx.session.role);
        resp.cmd_type = CMD_PERMISSION_ERR;
        resp.payload = "Insufficient permissions";
        return resp;
    }

    try {
        resp = entr.fn(ctx);
    } catch (const std::exception& e) {
        print_log(error, "Exception while handling cmd 0x%X: %s", ctx.body.cmd_type(), e.what());
        resp.cmd_type = CMD_SERVER_ERROR;
        resp.payload = "Internal server error";
    }

    return resp;
}

Error::ErrorInfo Dispatcher::send_response_s(TcpSocket::SocketHandler& sh, const Response_t& resp)
{
    Error::ErrorInfo err;
    // 检查指令
    if (!is_s2c(resp.cmd_type))
    {
        err.e = Error::CMD_ERR;
        err.message = "Response cmd_type must be S2C";
        print_log(err, info);
        return err;
    }

    if (resp.payload.empty())
    {
        err.e = Error::DISPATCH_ERR;
        err.message = "Response payload is empty";
        print_log(info, "Dispatcher::payload is empty");
        return err;
    }
    if (sh.socket_send(resp.payload.data(), resp.payload.size()).e != Error::SUCCESS)
    {
        err.e = Error::SEND_ERR;
        err.message = "Failed to send response";
        print_log(info, "Dispatcher::send_response failed");
    }
    return err;
}


Error::ErrorInfo Dispatcher::send_response_c(TcpSocket::SocketHandler& sh, const Response_t& resp)
{
    Error::ErrorInfo err;
    // 检查指令
    if (!is_c2s(resp.cmd_type))
    {
        err.e = Error::CMD_ERR;
        err.message = "Response cmd_type must be C2S";
        print_log(err, info);
        return err;
    }

    if (resp.payload.empty())
    {
        err.e = Error::DISPATCH_ERR;
        err.message = "Response payload is empty";
        print_log(info, "Dispatcher::payload is empty");
        return err;
    }
    if (sh.socket_send(resp.payload.data(), resp.payload.size()).e != Error::SUCCESS)
    {
        err.e = Error::SEND_ERR;
        err.message = "Failed to send response";
        print_log(info, "Dispatcher::send_response failed");
    }
    return err;
}

