#pragma once
#include <cstdint>

namespace Protocal
{
    constexpr uint32_t C2S = 0x00000000;
    constexpr uint32_t S2C = 0xF0000000;
    enum cmdtype_t : uint32_t {
        // Authentication
        CMD_LOGIN_REQ               = C2S | 0x0001,
        CMD_LOGIN_RESP              = S2C | 0x0002,
        CMD_LOGOUT_REQ              = C2S | 0x0003,
        // Queries
        CMD_QUERY_BY_CODE_REQ       = C2S | 0x0101,
        CMD_QUERY_BY_INSTRUCTOR_REQ = C2S | 0x0102,
        CMD_QUERY_BY_SEMESTER_REQ   = C2S | 0x0103,
        CMD_VIEW_ALL_REQ            = C2S | 0x0104,
        CMD_VIEW_ALL_PAGE_REQ       = C2S | 0x0105,
        CMD_QUERY_RESP              = S2C | 0x0180,
        // Admin (requires ADMIN role)
        CMD_ADD_REQ                 = C2S | 0x0201,
        CMD_UPDATE_REQ              = C2S | 0x0202,
        CMD_DELETE_REQ              = C2S | 0x0203,
        CMD_ADMIN_RESP              = S2C | 0x0280,
        // Admin account management (requires ADMIN role)
        CMD_ADMIN_LIST_REQ          = C2S | 0x0210,
        CMD_ADMIN_CREATE_REQ        = C2S | 0x0211,
        CMD_ADMIN_UPDATE_REQ        = C2S | 0x0212,
        CMD_ADMIN_DELETE_REQ        = C2S | 0x0213,
        CMD_ADMIN_LIST_RESP         = S2C | 0x0290,
        // Common responses / errors
        CMD_OK                      = S2C | 0xFF00,
        CMD_ERROR                   = S2C | 0xFFFF,
        CMD_PERMISSION_ERR          = S2C | 0xFFFE,
        CMD_SERVER_ERROR            = S2C | 0xFFFD,
    };
    constexpr bool is_c2s(const uint32_t c) { return (c & 0x80000000) == 0; }
    constexpr bool is_s2c(const uint32_t c) { return (c & 0x80000000) != 0; }

}

