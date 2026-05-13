#pragma once
#include "CmdHandler.h"
using namespace Protocal::Dispatch;

Response_t handle_login(const ReqContext_t& ctx);
Response_t handle_logout(const ReqContext_t& ctx);
Response_t handle_query_code(const ReqContext_t& ctx);
Response_t handle_query_instructor(const ReqContext_t& ctx);
Response_t handle_query_semester(const ReqContext_t& ctx);
Response_t handle_add(const ReqContext_t& ctx);
Response_t handle_update(const ReqContext_t& ctx);
Response_t handle_delete(const ReqContext_t& ctx);

/**
 * 命令注册器
 * @param dispatcher 命令调度器实例
 */
void register_all_server(Dispatcher& dispatcher);

