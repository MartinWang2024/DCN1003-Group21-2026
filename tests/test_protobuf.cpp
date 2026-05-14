#include "message.pb.h"
#include "test.h"

// ─────────────────────────────────────────────
// Protobuf 序列化 / 反序列化测试
// Proto 结构:
//   message Payload  { repeated bytes json = 1; }
//   message MsgBody  { uint32 cmd_type=1; uint32 req_id=2;
//                      uint32 timestamp=3; Payload payload=4; }
// ─────────────────────────────────────────────

TEST(test_protobuf_pack)
{
    MsgBody body;
    body.set_cmd_type(1);
    body.set_req_id(999);

    Payload* payload = body.mutable_payload();
    payload->add_json(R"({"course":"Math"})");

    std::string binary_data;
    body.SerializeToString(&binary_data);

    std::cout << "打包成功！二进制数据的长度是: " << binary_data.length() << " 字节" << std::endl;
    REQUIRE(!binary_data.empty());
}

TEST(test_protobuf_unpack)
{
    // ── 构建消息 ──────────────────────────────
    MsgBody body;
    body.set_cmd_type(2);
    body.set_req_id(10086);
    body.set_timestamp(1698144000);

    Payload* payload = body.mutable_payload();
    payload->add_json(R"({"course":"Math","score":85})");
    payload->add_json(R"({"course":"English","score":92})");
    payload->add_json(R"({"course":"Computer Science","score":78})");

    // ── 序列化 ────────────────────────────────
    std::string binary_data;
    body.SerializeToString(&binary_data);

    // ── 反序列化 ──────────────────────────────
    MsgBody received;
    REQUIRE(received.ParseFromString(binary_data));

    // ── 验证标量字段 ──────────────────────────
    REQUIRE(received.cmd_type()  == 2);
    REQUIRE(received.req_id()    == 10086);
    REQUIRE(received.timestamp() == 1698144000);

    // ── 验证 payload.json 数组 ─────────────────
    REQUIRE(received.payload().json_size() == 3);
    REQUIRE(received.payload().json(0) == R"({"course":"Math","score":85})");
    REQUIRE(received.payload().json(1) == R"({"course":"English","score":92})");
    REQUIRE(received.payload().json(2) == R"({"course":"Computer Science","score":78})");
}

TEST(test_build_up_msg)
{
    // 构建一条完整消息并验证序列化 / 反序列化往返一致
    MsgBody body;
    body.set_cmd_type(3);
    body.set_req_id(42);
    body.set_timestamp(0);
    body.mutable_payload()->add_json(R"({"key":"value"})");

    std::string bin;
    REQUIRE(body.SerializeToString(&bin));

    MsgBody copy;
    REQUIRE(copy.ParseFromString(bin));
    REQUIRE(copy.cmd_type()            == 3);
    REQUIRE(copy.req_id()              == 42);
    REQUIRE(copy.payload().json_size() == 1);
    REQUIRE(copy.payload().json(0)     == R"({"key":"value"})");
}

TEST(test_empty_payload)
{
    // payload 为空时仍能正常序列化 / 反序列化
    MsgBody body;
    body.set_req_id(1);

    std::string bin;
    REQUIRE(body.SerializeToString(&bin));

    MsgBody copy;
    REQUIRE(copy.ParseFromString(bin));
    REQUIRE(copy.req_id()              == 1);
    REQUIRE(copy.payload().json_size() == 0);
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    std::cout << "--- Serialization Testing ---\n";
    RUN(test_protobuf_pack);
    RUN(test_protobuf_unpack);
    RUN(test_build_up_msg);
    RUN(test_empty_payload);

    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}

