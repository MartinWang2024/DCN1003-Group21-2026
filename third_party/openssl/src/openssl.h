#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <winsock2.h> // 必须在 openssl 之前
#include "../libopenssl/include/openssl/evp.h"
#include "../libopenssl/include/openssl/sha.h"
#include "../libopenssl/include/openssl/err.h"
#include "../libopenssl/include/openssl/rand.h"

#include "../../../Driver/include/error.h"

namespace openssl
{

std::string sha256(const std::string& str);
/**
 * AES加密
 * @param plain_text 要加密的明文
 * @param key 密钥
 * @param iv 初始向量
 * @return 密文
 */
std::vector<unsigned char> aes_encrypt(
    const std::string& plain_text,
    const unsigned char* key,
    const unsigned char* iv);

/**
 * AES解密
 * @param cipher_text 待解密密文
 * @param key 密钥
 * @param iv 初始向量
 * @return 明文
 */
std::vector<unsigned char> aes_decrypt(
    const std::string& cipher_text,
    const unsigned char* key,
    const unsigned char* iv);

/**
 * IV生成
 * @param length 生成长度
 * @param iv 输出变量
 * @return 生成结果 如果为 nullptr 则失败
 *
 */
bool iv_gen(unsigned char* iv, size_t length);

/**
 * 使用sha256进行mac校验
 * @param key 密钥
 * @param key_len 密钥长度
 * @param data 待计算MAC数据
 * @param data_len 数据长度
 * @param out_mac MAC输出地址
 * @param out_len MAC实际长度
 */
void compute_hmac(
    const unsigned char* key, size_t key_len,
    const unsigned char* data, size_t data_len,
    unsigned char* out_mac,
    size_t* out_len);

Error::ErrorInfo readAppKey(unsigned char* key, const char* key_path = "app.key");

inline unsigned char* key = {nullptr};


}
