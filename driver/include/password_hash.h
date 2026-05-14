#pragma once

#include <string>

namespace dcn_database {

// PBKDF2-HMAC-SHA256 with random 16-byte salt.
// 编码格式: pbkdf2$<iter>$<saltHex>$<hashHex>
//   iter    十进制迭代次数 (默认 120000)
//   saltHex 16字节盐, 32 hex
//   hashHex 32字节派生密钥, 64 hex
// 旧明文密码视为合法但已过时, verify_login() 命中后由调用方触发 rehash 重写.
struct PasswordHash {
    static std::string make(const std::string& password,
                            int iterations = 120000);
    static bool verify(const std::string& password,
                       const std::string& encoded);
    static bool is_encoded(const std::string& s);
};

}  // namespace dcn_database
