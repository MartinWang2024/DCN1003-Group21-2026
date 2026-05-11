#include "protocol.h"
#include "config.h"
#include "log.h"

Error::ErrorInfo Protocal::Package_send(TcpSocket::SocketHandler sh, const unsigned char* msg, uint32_t cmd_type)
{
    Error::ErrorInfo err;
    // 检查有没有把 key load 进内存
    if (openssl::key == nullptr)
    {
        if (openssl::readAppKey(openssl::key, "app,key").e != 0)
        {
            err.e = Error::READ_ERR;
            err.message = "retry load key to memory failed.";
            print_log(err, debug);
            return err;
        }
    }

    MsgHeader header;
    header.version = std::stoi(APP_VERSION);

    // 计算消息 mac 值
    size_t mac_size;
    openssl::compute_hmac(
        openssl::key,
        sizeof(openssl::key),
        msg,
        sizeof(msg) - 1,
        header.mac,
        &mac_size);

    // 预期将生成32byte大小的mac值
    if (mac_size != sizeof(openssl::key))
    {
        err.e = Error::MAC_CALC_ERR;
        err.message = "mac calc failed.";
        print_log(err, debug);
        return err;
    }

    // 计算AES初始向量
    unsigned char iv[16] = {0};
    if (!openssl::iv_gen(iv, sizeof(iv)))
    {
        err.e = Error::IV_CALC_ERR;
        err.message = "generated iv failed.";
        print_log(err, debug);
        return err;
    }
    memcpy(header.iv, iv, sizeof(header.iv));

    // 填充body
    MsgBody body;
    body.set_cmd_type(cmd_type);
    body.set_timestamp(get_now_time());
    body.set_req_id(*"114514");

    // 填充payload
    Payload* payload_ptr = body.mutable_payload();
    std::string msg_str(reinterpret_cast<const char*>(msg));
    payload_ptr->add_json(msg_str);

    // 序列化 MsgBody
    std::string body_serialized_data;
    body.SerializeToString(&body_serialized_data);

    // 加密序列化后的 body，
    std::vector<unsigned char> ciphertext = openssl::aes_encrypt(
        body_serialized_data,
        openssl::key,
        iv);

    // 获取加密后信息长度
    header.body_len = ciphertext.size();

    // 发送
    if (((sh.socket_send(&header, sizeof(header)).e) &&
        (sh.socket_send(ciphertext.data(), ciphertext.size()).e)) != Error::SUCCESS)
    {
        err.e = Error::SOCKET_ERR;
        err.message = "Package_send: package failed.";
        print_log(err, debug);
        return err;
    }

    //     sh.socket_send(&header, sizeof(header));
    // sh.socket_send(ciphertext.data(), ciphertext.size());

    return err;
}
