#include "protocol.h"
#include "config.h"
#include "log.h"
#include <atomic>

namespace {
    std::atomic<uint32_t> g_req_id_counter{1};
}

Error::ErrorInfo Protocal::Package_send(TcpSocket::SocketHandler sh, const u_char* msg, size_t msg_len, uint32_t cmd_type)
{
    Error::ErrorInfo err;

    unsigned char zero_buffer[32] = {0};

    // 检查有没有把 key load 进内存
    if (memcmp(openssl::key, zero_buffer, sizeof(openssl::key)) == 0) {
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

    // 填充body
    MsgBody body;
    body.set_cmd_type(cmd_type);
    body.set_timestamp(get_now_time());
    // 单调自增的请求号，避免硬编码字面量并保证包间唯一
    body.set_req_id(g_req_id_counter.fetch_add(1, std::memory_order_relaxed));

    // 填充payload（按真实长度构造，避免 msg 中含 \0 时被截断）
    Payload* payload_ptr = body.mutable_payload();
    std::string msg_str(reinterpret_cast<const char*>(msg), msg_len);
    payload_ptr->add_json(msg_str);

    // 序列化 MsgBody
    std::string body_serialized_data;
    body.SerializeToString(&body_serialized_data);

    // 计算消息 mac 值
    size_t mac_size;
    openssl::compute_hmac(
        openssl::key,
        sizeof(openssl::key),
        // 对序列化后的body计算mac校验值
        reinterpret_cast<const unsigned char*>(body_serialized_data.data()),
        body_serialized_data.length(),
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
    u_char iv[16] = {0};
    if (!openssl::iv_gen(iv, sizeof(iv)))
    {
        err.e = Error::IV_CALC_ERR;
        err.message = "generated iv failed.";
        print_log(err, debug);
        return err;
    }
    memcpy(header.iv, iv, sizeof(header.iv));

    // 加密序列化后的 body，
    std::vector<u_char> ciphertext = openssl::aes_encrypt(
        body_serialized_data,
        openssl::key,
        iv);
    if (ciphertext.empty())
    {
        err.e = Error::ENCRYPT_ERR;
        err.message = "aes encrypt failed.";
        print_log(err, debug);
        return err;
    }

    // 获取加密后信息长度
    header.body_len = ciphertext.size();

    // 分步发送 header 与 body，任一失败即返回（原 `&&` 短路写法会漏判单边失败）
    Error::ErrorInfo header_err = sh.socket_send(&header, sizeof(header));
    if (header_err.e != Error::SUCCESS)
    {
        err.e = Error::SEND_ERR;
        err.message = "Package_send: send header failed.";
        print_log(err, debug);
        return err;
    }

    Error::ErrorInfo body_err = sh.socket_send(ciphertext.data(), ciphertext.size());
    if (body_err.e != Error::SUCCESS)
    {
        err.e = Error::SEND_ERR;
        err.message = "Package_send: send body failed.";
        print_log(err, debug);
        return err;
    }

    return err;
}

Error::ErrorInfo Protocal::Package_receive(TcpSocket::SocketHandler sh, google::protobuf::Message& msg)
{
    Error::ErrorInfo err;
    MsgHeader recv_header;

    // 接收包头
    if (sh.socket_recv(&recv_header, sizeof(MsgHeader)).e != Error::SUCCESS)
    {
        err.e = Error::RECV_ERR;
        err.message = "Package header receive failed.";
        print_log(err, debug);
        return err;
    }
    // 协议版本校验
    if (recv_header.version != std::stoi(APP_VERSION))
    {
        err.e = Error::RECV_ERR;
        err.message = "Unsupported protocol version.";
        print_log(err, debug);
        return err;
    }
    // 接收包体
    if (recv_header.body_len == 0 || recv_header.body_len > MAX_BODY)
    {
        err.e = Error::RECV_ERR;
        err.message = "Package body length = 0 or too long.";
        print_log(err, debug);
        return err;
    }
    std::string buffer;
    buffer.resize(recv_header.body_len);
    if ((sh.socket_recv(buffer.data(), recv_header.body_len)).e != Error::SUCCESS)
    {
        err.e = Error::RECV_ERR;
        err.message = "Package body receive failed.";
        print_log(err, debug);
    }
    // 包体解码
    std::vector<u_char> plaintext = openssl::aes_decrypt(
        buffer,
        openssl::key,
        recv_header.iv);
    if (plaintext.empty())
    {
        err.e = Error::DECRYPT_ERR;
        err.message = "aes decrypt failed.";
        print_log(err, debug);
        return err;
    }

    // 执行MAC校验
    uint8_t recv_calc_mac[32] = {0};
    size_t mac_size;
    openssl::compute_hmac(
        openssl::key,sizeof(openssl::key),
        plaintext.data(), plaintext.size(),
        recv_calc_mac,
        &mac_size);

    if (mac_size != sizeof(openssl::key))
    {
        err.e = Error::MAC_CALC_ERR;
        err.message = "mac calc failed.";
        print_log(err, debug);
        return err;
    }

    if (memcmp(recv_calc_mac, recv_header.mac, sizeof(recv_header.mac)) != 0)
    {
        err.e = Error::MAC_CHECK_ERR;
        err.message = "mac check failed: recv mac != body calc mac";
        print_log(err, debug);
        return err;
    }

    if (!msg.ParseFromArray(plaintext.data(), plaintext.size()))
    {
        err.e = Error::PROTO_PARSE_ERR;
        err.message = "protobuf parse failed.";
        print_log(err, debug);
        return err;
    }


    return err;
}
