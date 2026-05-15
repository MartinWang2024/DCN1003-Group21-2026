#include "protocol.h"
#include "config.h"
#include "log.h"
#include <atomic>

namespace {

Error::ErrorInfo send_body(TcpSocket::SocketHandler& sh, MsgBody& body)
{
    Error::ErrorInfo err;

    unsigned char zero_buffer[32] = {0};
    if (memcmp(openssl::key, zero_buffer, sizeof(openssl::key)) == 0) {
        if (openssl::readAppKey(openssl::key, "app.key").e != 0) {
            err.e = Error::READ_ERR;
            err.message = "retry load key to memory failed.";
            print_log(err, debug);
            return err;
        }
    }

    Protocal::MsgHeader_t header;
    header.version = std::stoi(APP_VERSION);
    body.set_timestamp(get_now_time());
    body.set_req_id(Protocal::detail::g_req_id_counter.fetch_add(1, std::memory_order_relaxed));

    std::string body_serialized_data;
    body.SerializeToString(&body_serialized_data);

    size_t mac_size;
    openssl::compute_hmac(
        openssl::key, sizeof(openssl::key),
        reinterpret_cast<const unsigned char*>(body_serialized_data.data()),
        body_serialized_data.length(),
        header.mac, &mac_size);
    if (mac_size != sizeof(openssl::key)) {
        err.e = Error::MAC_CALC_ERR;
        err.message = "mac calc failed.";
        print_log(err, debug);
        return err;
    }

    u_char iv[16] = {0};
    if (!openssl::iv_gen(iv, sizeof(iv))) {
        err.e = Error::IV_CALC_ERR;
        err.message = "generated iv failed.";
        print_log(err, debug);
        return err;
    }
    memcpy(header.iv, iv, sizeof(header.iv));

    std::vector<u_char> ciphertext = openssl::aes_encrypt(body_serialized_data, openssl::key, iv);
    if (ciphertext.empty()) {
        err.e = Error::ENCRYPT_ERR;
        err.message = "aes encrypt failed.";
        print_log(err, debug);
        return err;
    }
    header.body_len = ciphertext.size();

    if (sh.socket_send(&header, sizeof(header)).e != Error::SUCCESS) {
        err.e = Error::SEND_ERR;
        err.message = "Package_send: send header failed.";
        print_log(err, debug);
        return err;
    }
    if (sh.socket_send(ciphertext.data(), ciphertext.size()).e != Error::SUCCESS) {
        err.e = Error::SEND_ERR;
        err.message = "Package_send: send body failed.";
        print_log(err, debug);
        return err;
    }
    return err;
}

}  // namespace

Error::ErrorInfo Protocal::Package_send(TcpSocket::SocketHandler &sh, const u_char* msg, size_t msg_len, uint32_t cmd_type)
{
    MsgBody body;
    body.set_cmd_type(cmd_type);
    Payload* payload_ptr = body.mutable_payload();
    payload_ptr->add_json(std::string(reinterpret_cast<const char*>(msg), msg_len));
    return send_body(sh, body);
}

Error::ErrorInfo Protocal::Package_send(TcpSocket::SocketHandler &sh, const std::vector<std::string>& fields, uint32_t cmd_type)
{
    MsgBody body;
    body.set_cmd_type(cmd_type);
    Payload* payload_ptr = body.mutable_payload();
    for (const auto& f : fields) payload_ptr->add_json(f);
    return send_body(sh, body);
}

Error::ErrorInfo Protocal::Package_receive(TcpSocket::SocketHandler &sh, google::protobuf::Message& msg)
{
    Error::ErrorInfo err;
    MsgHeader_t recv_header;

    // Receive header
    if (sh.socket_recv(&recv_header, sizeof(MsgHeader_t)).e != Error::SUCCESS)
    {
        err.e = Error::RECV_ERR;
        err.message = "Package header receive failed.";
        print_log(err, debug);
        return err;
    }
    // Verify protocol version
    if (recv_header.version != std::stoi(APP_VERSION))
    {
        err.e = Error::RECV_ERR;
        err.message = "Unsupported protocol version.";
        print_log(err, debug);
        return err;
    }
    // Receive body
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
        return err;
    }
    // Decode body
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

    // Verify MAC
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
