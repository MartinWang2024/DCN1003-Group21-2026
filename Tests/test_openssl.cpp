#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../third_party/openssl/src/openssl.h"
#include "test.h"

TEST(test_openssl_sha256) {
    std::string input = "hello world";
    std::string expected_hash = "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9";
    REQUIRE(openssl::sha256(input) == expected_hash);
}

TEST(test_openssl_aes_encrypt) {
    std::string plain_text = "This is a secret message.";
    unsigned char key[32] = {0}; // 256-bit key (all zeros for testing)
    unsigned char iv[16] = {0};  // 128-bit IV (all zeros for testing)

    std::vector<unsigned char> ciphertext = openssl::aes_encrypt(plain_text, key, iv);
    REQUIRE(!ciphertext.empty());
}

TEST(test_openssl_readkey)
{
    unsigned char key[32] = {0};
    Error::ErrorInfo err = openssl::readAppKey(key);
    REQUIRE(err.e == Error::SUCCESS);
}

TEST(test_openssl_calculate_msg_mac)
{
    unsigned char key[32] = {0};
    unsigned char msg[] = "This is a secret message.";
    unsigned char out_mac[32] = {0};
    size_t mac_size;

    Error::ErrorInfo err = openssl::readAppKey(key);
    REQUIRE(err.e == Error::SUCCESS);
    openssl::compute_hmac(
    key,
    sizeof(key),
    msg,
    sizeof(msg)-1,
    out_mac,
    &mac_size);

    std::cout << "MAC result (hex): ";
    for (size_t i = 0; i < mac_size; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)out_mac[i];
    }
    std::cout << std::dec << std::endl;

    // std::cout << "result:" << out_mac << std::endl;
    REQUIRE(mac_size == 32);
    bool all_zero = true;
    for (size_t i = 0; i < mac_size; i++) {
        if (out_mac[i] != 0) {
            all_zero = false;
            break;
        }
    }
    REQUIRE(!all_zero);
}

TEST(test_iv_gen)
{
    unsigned char iv[16] = {0};
    REQUIRE(openssl::iv_gen(iv, sizeof(iv)));
    std::cout << "generated iv: " << iv << std::endl;
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

    std::cout << "--- test calculate key ---\n";
    RUN(test_openssl_readkey);
    RUN(test_openssl_calculate_msg_mac);
    RUN(test_iv_gen);

    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}
