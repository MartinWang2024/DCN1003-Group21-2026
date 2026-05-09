#pragma once

std::string sha256(const std::string& str);
/**
 *
 * @param plain_text 要加密的明文
 * @param key 密钥
 * @param iv 初始向量
 * @return 密文
 */
std::vector<unsigned char> aes_encrypt(const std::string& plain_text, const unsigned char* key, const unsigned char* iv);