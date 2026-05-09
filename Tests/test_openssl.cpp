#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../third_party/openssl/src/openssl.h"
#include "test.h"

TEST(test_openssl_sha256) {
    std::string input = "hello world";
    std::string expected_hash = "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9";
    REQUIRE(sha256(input) == expected_hash);
}

TEST(test_openssl_aes_encrypt) {
    std::string plain_text = "This is a secret message.";
    unsigned char key[32] = {0}; // 256-bit key (all zeros for testing)
    unsigned char iv[16] = {0};  // 128-bit IV (all zeros for testing)

    std::vector<unsigned char> ciphertext = aes_encrypt(plain_text, key, iv);
    REQUIRE(!ciphertext.empty());
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    std::cout << "=== File-based Database Tests ===\n\n";

    std::cout << "--- test sha256 ---\n";
    RUN(test_openssl_sha256);

    std::cout << "--- test aes encypt ---\n";
    RUN(test_openssl_aes_encrypt);

    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}
