#include "openssl.h"

#include <fstream>

#include "../../../Driver/include/error.h"

namespace openssl
{


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

std::vector<unsigned char> aes_decrypt(const std::string& cipher_text, const unsigned char* key, const unsigned char* iv)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    std::vector<unsigned char> plaintext(cipher_text.size() + 16);
    int len;
    int plaintext_len;
    // TODO

}

bool iv_gen(unsigned char* iv, size_t length)
{
    if (RAND_bytes(iv,length) != 1)
    {
        printf("RAND_bytes failed\n");
        return false;
    }
    return true;
}

void compute_hmac(const unsigned char* key, size_t key_len,
                  const unsigned char* data, size_t data_len,
                  unsigned char* out_mac, size_t* out_len) {

    // 创建Context
    EVP_MAC *mac = EVP_MAC_fetch(NULL, "HMAC", NULL);
    EVP_MAC_CTX *ctx = EVP_MAC_CTX_new(mac);

    // 设置参数
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string("digest", (char*)"SHA256", 0);
    params[1] = OSSL_PARAM_construct_end();

    // init
    EVP_MAC_init(ctx, key, key_len, params);

    // set Encryption data
    EVP_MAC_update(ctx, data, data_len);

    // get Encryption result
    EVP_MAC_final(ctx, out_mac, out_len, 32);

    // clean memory
    EVP_MAC_CTX_free(ctx);
    EVP_MAC_free(mac);
}

Error::ErrorInfo readAppKey(unsigned char* key, const char* key_path)
{
    Error::ErrorInfo err;
    // static_cast<char*>(key);
    // const std::string filename = "app.key";
    const int key_size = 32;

    // 以二进制模式打开文件
    std::ifstream key_file(key_path, std::ios::binary);

    if (!key_file) {
        err.message = "Unable to open key file";
        err.e = Error::READ_ERR;
        return err;
    }
    unsigned char buffer[key_size];

    // read key
    key_file.read(reinterpret_cast<char*>(buffer), key_size);

    // 检查是否真的读到了 32 字节
    if (key_file.gcount() != key_size) {
        err.message = "key file size is not 32 byte!";
        err.e = Error::READ_ERR;
        return err;
    }
    key_file.close();

    // std::cout << "read success! key is" << std::endl;
    // for (int i = 0; i < key_size; i++) {std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)buffer[i] << " ";}
    // std::cout << std::dec << std::endl;

    memcpy(key, buffer, key_size);
    for(int i = 0; i < key_size; i++) buffer[i] = 0;

    return err;
}
}
