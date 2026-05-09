#include "message.pb.h"
#include "test.h"



TEST(test_protobuf_pack)
{
    // 1. 创建一个消息对象
    MsgBody body;
    body.set_req_id(999);

    // 2. 往数组（repeated）里加点东西
    Payload* payload = body.mutable_payload();
    payload->add_selected_courses("Math");

    // 3. 序列化：变成二进制字符串
    std::string binary_data;
    body.SerializeToString(&binary_data);

    std::cout << "打包成功！二进制数据的长度是: " << binary_data.length() << " 字节" << std::endl;
}

TEST(test_protobuf_unpack)
{
    MsgBody body;

    // 2. 设置普通变量
    body.set_req_id(10086);
    body.set_timestamp(1698144000);

    // 3. 拿到里面的 payload 对象的指针
    Payload* payload = body.mutable_payload();
    payload->set_username("stu001");

    // 每次调用 add_ ，就会往数组里塞入一个新的元素
    payload->add_selected_courses("Math");
    payload->add_selected_courses("English");
    payload->add_selected_courses("Computer Science");

    payload->add_scores(85);
    payload->add_scores(92);
    payload->add_scores(78);

    // 5. 将这些数据序列化
    std::string binary_data;
    body.SerializeToString(&binary_data);


    MsgBody received_body;
    // 把收到的二进制串解包反序列化
    received_body.ParseFromString(binary_data);

    REQUIRE(received_body.req_id() == 10086);
    REQUIRE(received_body.timestamp() == 1698144000);

    int list[3] = {85, 92, 78};
    for (int i = 0; i < received_body.payload().scores_size(); i++) {
        REQUIRE(received_body.payload().scores(i) == list[i]);
    }
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    std::cout << "--- Serialization Testing ---\n";
    RUN(test_protobuf_pack);
    RUN(test_protobuf_unpack);

    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}
