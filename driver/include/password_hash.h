#pragma once

#include <string>

namespace dcn_database {

// PBKDF2-HMAC-SHA256 with random 16-byte salt.
// Encoding: pbkdf2$<iter>$<saltHex>$<hashHex>
//   iter    decimal iteration count (default 120000)
//   saltHex 16-byte salt, 32 hex chars
//   hashHex 32-byte derived key, 64 hex chars
// Legacy plaintext passwords are accepted but obsolete; verify_login() triggers
// an in-place upgrade to PBKDF2 on match via the caller.
struct PasswordHash {
    static std::string make(const std::string& password,
                            int iterations = 120000);
    static bool verify(const std::string& password,
                       const std::string& encoded);
    static bool is_encoded(const std::string& s);
};

}  // namespace dcn_database
