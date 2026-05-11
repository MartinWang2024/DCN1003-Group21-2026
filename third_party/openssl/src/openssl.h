#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <winsock2.h> // 必须在 openssl 之前
#include "../libopenssl/include/openssl/evp.h"
#include "../libopenssl/include/openssl/sha.h"
#include "../libopenssl/include/openssl/err.h"

std::string sha256(const std::string& str);
/**
 *
 * @param plain_text 要加密的明文
 * @param key 密钥
 * @param iv 初始向量
 * @return 密文
 */
std::vector<unsigned char> aes_encrypt(const std::string& plain_text, const unsigned char* key, const unsigned char* iv);

std::vector<unsigned char> aes_decrypt(const std::string& cipher_text, const unsigned char* key, const unsigned char* iv);