#pragma once
#include <cstdint>

namespace Protocal
{
    constexpr uint32_t C2S = 0x00000000;
    constexpr uint32_t S2C = 0xF0000000;
    enum cmdtype_t : uint32_t {
        // 认证类
        CMD_LOGIN_REQ               = C2S | 0x0001,   // 协议请求: 登录
        CMD_LOGIN_RESP              = S2C | 0x0002,   // 协议响应: 登录
        CMD_LOGOUT_REQ              = C2S | 0x0003,   // 协议请求: 登出
        // 查询类
        CMD_QUERY_BY_CODE_REQ       = C2S | 0x0101,   // 协议请求: 课程编码查询
        CMD_QUERY_BY_INSTRUCTOR_REQ = C2S | 0x0102,   // 协议请求: 导师姓名查询
        CMD_QUERY_BY_SEMESTER_REQ   = C2S | 0x0103,   // 协议请求: 学期编号查询
        CMD_VIEW_ALL_REQ            = C2S | 0x0104,   // 协议请求: 查询全部课程
        CMD_VIEW_ALL_PAGE_REQ       = C2S | 0x0105,   // 协议请求: 分页查询全部课程
        CMD_QUERY_RESP              = S2C | 0x0180,   // 协议响应: 查询
        // 管理类（需 admin 权限）
        CMD_ADD_REQ                 = C2S | 0x0201,   // 协议请求: 增加
        CMD_UPDATE_REQ              = C2S | 0x0202,   // 协议请求: 变更
        CMD_DELETE_REQ              = C2S | 0x0203,   // 协议请求: 删除
        CMD_ADMIN_RESP              = S2C | 0x0280,   // 协议响应: 管理员
        // 通用响应 / 错误
        CMD_OK                      = S2C | 0xFF00,   // 协议响应: 成功
        CMD_ERROR                   = S2C | 0xFFFF,   // 协议响应: 失败
        CMD_PERMISSION_ERR          = S2C | 0xFFFE,   // 协议响应: 权限不足
        CMD_SERVER_ERROR            = S2C | 0xFFFD,   // 协议响应: 服务器内部错误
    };
    constexpr bool is_c2s(const uint32_t c) { return (c & 0x80000000) == 0; }
    constexpr bool is_s2c(const uint32_t c) { return (c & 0x80000000) != 0; }

}

