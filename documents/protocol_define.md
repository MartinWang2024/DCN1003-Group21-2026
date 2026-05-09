## 数据帧定义

包头

```c++
struct MsgHeader
{
    uint16_t version;
    uint16_t cmd_type;
    uint16_t body_len;
    uint16_t hash[16]; // 256位哈希
    uint16_t align;
}
```

包体

```c++
struct MsgBody
{
    int req_id;
    int timestamp;
    struct data{}
}
```

包体中包含多种响应结构

login结构体

```c++
struct reg
{
    uint16_t username[32];
    uint16_t pwd_hash[16]; // 256位密码哈希
}
```

