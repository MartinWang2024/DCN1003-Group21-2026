#include "openssl.h"

std::string sha256(const std::string& str) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();

    EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(mdctx, str.c_str(), str.size());
    EVP_DigestFinal_ex(mdctx, hash, nullptr);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

std::vector<unsigned char> aes_encrypt(const std::string& plain_text, const unsigned char* key, const unsigned char* iv) {
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();

    // AES 每块 16 字节，加密后可能变长，所以预留足够空间
    std::vector<unsigned char> ciphertext(plain_text.size() + 16);
    int len;
    int ciphertext_len;

    // 初始化加密上下文
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv);

    // 加密
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len, (unsigned char*)plain_text.c_str(), plain_text.size());
    ciphertext_len = len;

    // 结束处理（处理填充 padding）
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    ciphertext_len += len;

    ciphertext.resize(ciphertext_len); // 缩减到实际长度
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext;
}