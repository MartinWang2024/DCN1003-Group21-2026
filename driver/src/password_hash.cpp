#include "password_hash.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <vector>

namespace dcn_database {

namespace {

constexpr int kSaltLen = 16;
constexpr int kKeyLen  = 32;

std::string to_hex(const unsigned char* data, std::size_t len) {
    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < len; ++i) {
        os << std::setw(2) << static_cast<int>(data[i]);
    }
    return os.str();
}

bool from_hex(const std::string& hex, std::vector<unsigned char>& out) {
    if (hex.size() % 2 != 0) return false;
    out.resize(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        unsigned int b = 0;
        if (std::sscanf(hex.c_str() + i * 2, "%2x", &b) != 1) return false;
        out[i] = static_cast<unsigned char>(b);
    }
    return true;
}

// 常时比较, 防止时序侧信道
bool consteq(const std::vector<unsigned char>& a,
             const std::vector<unsigned char>& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) diff |= a[i] ^ b[i];
    return diff == 0;
}

bool pbkdf2(const std::string& password,
            const unsigned char* salt, int salt_len,
            int iter, unsigned char* out, int out_len) {
    return PKCS5_PBKDF2_HMAC(password.c_str(),
                             static_cast<int>(password.size()),
                             salt, salt_len, iter,
                             EVP_sha256(), out_len, out) == 1;
}

}  // namespace

bool PasswordHash::is_encoded(const std::string& s) {
    return s.compare(0, 7, "pbkdf2$") == 0;
}

std::string PasswordHash::make(const std::string& password, int iterations) {
    unsigned char salt[kSaltLen];
    if (RAND_bytes(salt, kSaltLen) != 1) return {};

    unsigned char key[kKeyLen];
    if (!pbkdf2(password, salt, kSaltLen, iterations, key, kKeyLen)) return {};

    std::ostringstream os;
    os << "pbkdf2$" << iterations << '$'
       << to_hex(salt, kSaltLen) << '$'
       << to_hex(key, kKeyLen);
    return os.str();
}

bool PasswordHash::verify(const std::string& password,
                          const std::string& encoded) {
    if (!is_encoded(encoded)) return false;

    auto p1 = encoded.find('$', 7);
    if (p1 == std::string::npos) return false;
    auto p2 = encoded.find('$', p1 + 1);
    if (p2 == std::string::npos) return false;

    int iter = 0;
    try {
        iter = std::stoi(encoded.substr(7, p1 - 7));
    } catch (...) {
        return false;
    }
    if (iter <= 0) return false;

    std::vector<unsigned char> salt;
    if (!from_hex(encoded.substr(p1 + 1, p2 - p1 - 1), salt)) return false;

    std::vector<unsigned char> expect;
    if (!from_hex(encoded.substr(p2 + 1), expect)) return false;
    if (expect.size() != kKeyLen) return false;

    std::vector<unsigned char> got(kKeyLen);
    if (!pbkdf2(password, salt.data(), static_cast<int>(salt.size()), iter,
                got.data(), kKeyLen)) {
        return false;
    }
    return consteq(got, expect);
}

}  // namespace dcn_database
