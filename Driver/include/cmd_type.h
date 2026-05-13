#pragma once

namespace Protocal
{
    enum CmdType : uint32_t {
        // 认证类
        CMD_LOGIN_REQ               = 0x0001,   // 协议请求: 登录
        CMD_LOGIN_RESP              = 0x0002,   // 协议响应: 登录
        // 查询类
        CMD_QUERY_BY_CODE_REQ       = 0x0101,   // 协议请求: 课程编码查询
        CMD_QUERY_BY_INSTRUCTOR_REQ = 0x0102,   // 协议请求: 导师姓名查询
        CMD_QUERY_BY_SEMESTER_REQ   = 0x0103,   // 协议请求: 学期编号查询
        CMD_QUERY_RESP              = 0x0180,   // 协议响应: 查询
        // 管理类（需 admin 权限）
        CMD_ADD_REQ                 = 0x0201,   // 协议请求: 增加
        CMD_UPDATE_REQ              = 0x0202,   // 协议请求: 变更
        CMD_DELETE_REQ              = 0x0203,   // 协议请求: 删除
        CMD_ADMIN_RESP              = 0x0280,   // 协议响应: 管理员
        // 通用响应 / 错误
        CMD_OK                      = 0xFF00,   // 协议响应: 成功
        CMD_ERROR                   = 0xFFFF,   // 协议响应: 失败
    };
}

