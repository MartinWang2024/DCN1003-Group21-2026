#include "protocol.h"
#include "config.h"

bool SendMessage(SOCKET sock, const google::protobuf::Message& msg, uint32_t cmd_type)
{
    // 序列化传入的消息
    std::string serialized_data;
    msg.SerializeToString(&serialized_data);

    //
    Protocal::MsgHeader header;
    header.body_len = serialized_data.size();
    header.version = std::stoi(APP_VERSION);
    header.cmd_type = cmd_type;
    return false;

}