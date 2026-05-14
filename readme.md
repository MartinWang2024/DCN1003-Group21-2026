# DCN1003 课程表管理系统 — Group 21

计算机网络课程设计项目，实现了一个基于 C/S 架构的课程表查询与管理系统，包含自定义应用层协议、加密传输、数据库持久化和跨平台 GUI 客户端。

---

## 系统架构

```
┌──────────────────────┐                         ┌──────────────────────┐
│   Flutter GUI 客户端  │  stdio (TSV 协议)       │   课程数据库 (SQLite)   │
│   (dcn_flutter_client)│────────┐       ┌────────│   data/DCN.db       │
└──────────────────────┘        │       │        │                      │
                                ▼       ▼        └──────────────────────┘
┌──────────────────────┐  ┌──────────────────┐            ▲
│   C++ REPL 客户端     │  │   C++ Server      │            │
│   (dcn_client)       │  │   (dcn_server)    │            │
└──────────┬───────────┘  └────────┬─────────┘            │
           │                       │                      │
           │  TCP + AES + HMAC     │                      │
           └───────────────────────┘                      │
                    ▲                                     │
                    │        ┌────────────────────────────┘
                    │        │
              ┌─────┴────────┴─────┐
              │   共享库 (Driver)   │
              │  · 协议编解码       │
              │  · AES 加解密       │
              │  · 命令分发         │
              │  · 数据库访问       │
              └────────────────────┘
```

### 项目结构

| 目录                    | 说明                                                        |
| ----------------------- | ----------------------------------------------------------- |
| `driver/`             | 共享库：协议编解码、AES/HMAC 加解密、命令分发器、数据库仓库 |
| `programs/Server/`    | TCP 服务端：多线程连接处理、命令注册与业务逻辑              |
| `programs/Client/`    | C++ 客户端：REPL 交互模式 + Bridge 子进程模式               |
| `dcn_flutter_client/` | Flutter GUI：通过 stdio 桥接 C++ 客户端                     |
| `third_party/`        | 第三方依赖：protobuf、libopenssl                            |
| `data/`               | SQLite 数据库文件                                           |
| `tests/`              | 单元测试                                                    |
| `documents/`          | 设计文档                                                    |

---

## 实现功能

### 1. 自定义应用层协议

基于 **Protobuf + C 结构体** 的混合帧格式：

```
+-------------------+  ← MsgHeader (56 字节，明文)
| version   (4B)    |  协议版本号
| body_len  (4B)    | 加密后密文长度
| iv        (16B)   | AES-256-CBC 初始向量
| mac       (32B)   | HMAC-SHA256 消息认证码
+-------------------+
|                   |  ← Encrypted Body (变长)
| AES-256-CBC 密文   |
| (Protobuf MsgBody) |
+-------------------+
```

**设计要点**：

- 包头明文传输，包含协议版本号用于兼容性校验
- 包体使用 **AES-256-CBC** 加密，每包随机生成 IV
- **HMAC-SHA256** 对明文 MsgBody 计算 MAC，接收端解密后校验，防篡改
- 共享密钥从 `app.key` 文件加载（32 字节），客户端与服务端使用同一密钥
- 包体最大 16 MiB (`MAX_BODY`)

**Protobuf 消息定义**（`message.proto`）：

```protobuf
message Payload {
    repeated bytes json = 1;  // 变长参数字段（位置索引寻址）
}
message MsgBody {
    uint32 cmd_type  = 1;  // 命令码
    uint32 req_id    = 2;  // 请求序列号（原子递增）
    uint32 timestamp = 3;  // 时间戳
    Payload payload  = 4;  // 业务负载
}
```

### 2. 命令分发架构

分层设计，核心调度器位于 `driver` 共享库中：

```
L4  连接循环 (per-thread)
    while(alive) { recv → dispatch → send }

L3  命令分发器 (Dispatcher)
    · 按 cmd_type 路由到注册的 Handler
    · 集中鉴权检查 (ADMIN / STUDENT)
    · 异常捕获 → CMD_SERVER_ERROR，保证不崩溃

L2  业务 Handler
    · 解析 Payload 字段 → 调用数据库 → 构造响应

L1  数据访问层
    · CourseRepository / AdministratorRepository
```

**支持的命令**：

| 命令                    | 权限    | 说明                                    |
| ----------------------- | ------- | --------------------------------------- |
| `LOGIN`               | Student | 管理员登录认证，成功后升级为 ADMIN 会话 |
| `LOGOUT`              | Student | 登出，会话降级为 STUDENT                |
| `QUERY_BY_CODE`       | Student | 按课程代码精确查询                      |
| `QUERY_BY_INSTRUCTOR` | Student | 按教师姓名模糊查询                      |
| `QUERY_BY_SEMESTER`   | Student | 按学期查询（预留）                      |
| `ADD`                 | Admin   | 新增课程及排课信息（事务写入）          |
| `UPDATE`              | Admin   | 更新课程信息（事务更新）                |
| `DELETE`              | Admin   | 级联删除课程及排课                      |

权限检查集中在 `Dispatcher::dispatch()` 中，Handler 内部无需关心鉴权。

### 3. 安全传输

| 机制     | 算法        | 说明                                     |
| -------- | ----------- | ---------------------------------------- |
| 对称加密 | AES-256-CBC | 包体加密，每包独立随机 IV                |
| 消息认证 | HMAC-SHA256 | 对明文序列化结果计算 MAC，防止篡改和重放 |
| 访问控制 | 会话角色    | STUDENT (只读查询) / ADMIN (增删改)      |
| 协议校验 | 版本号检查  | 接收端校验 version 字段，拒绝不兼容版本  |

收发流程：

1. **发送**：序列化 MsgBody → 计算 HMAC → 生成随机 IV → AES 加密 → 先发 Header 再发密文
2. **接收**：读 Header → 校验版本 → 读密文 → AES 解密 → 校验 HMAC → Protobuf 反序列化

### 4. 数据库设计

使用 **SQLite** 数据库，启用 WAL 模式和外键约束。

**`courses` 表**（课程基本信息）：

| 列             | 类型         | 约束         |
| -------------- | ------------ | ------------ |
| `code`       | VARCHAR(32)  | PK, NOT NULL |
| `section`    | VARCHAR(32)  | PK, NOT NULL |
| `title`      | VARCHAR(128) | NOT NULL     |
| `instructor` | VARCHAR(64)  | NOT NULL     |

**`schedules` 表**（排课信息）：

| 列              | 类型        | 约束            |
| --------------- | ----------- | --------------- |
| `course_code` | VARCHAR(32) | PK, FK→courses |
| `section`     | VARCHAR(32) | PK, FK→courses |
| `day`         | DATE        | PK              |
| `duration`    | VARCHAR(32) | PK              |
| `semester`    | VARCHAR(16) | PK              |
| `classroom`   | VARCHAR(64) | NOT NULL        |

通过 `ON DELETE CASCADE` 实现级联删除。增删改操作均使用显式事务（BEGIN/COMMIT/ROLLBACK）。

**`administrators` 表**：存储管理员用户名和密码。

| 列           | 类型         | 约束        |
| ------------ | ------------ | ----------- |
| `username` | VARCHAR(64)  | PRIMARY KEY |
| `password` | VARCHAR(255) | NOT NULL    |

### 5. 服务端多线程

- 使用 Winsock2 TCP，监听端口 9001
- 每个客户端连接创建一个独立的 `std::thread`（detached）
- 每线程运行 `handle_client` 循环：收包 → 分发 → 回包 → 直到连接断开
- 数据库和 Dispatcher 实例在线程间共享（`std::ref`），线程安全由 SQLite `FULLMUTEX` 保证

### 6. 双模式客户端

C++ 客户端支持两种运行模式：

- **REPL 模式** (`dcn_client.exe`)：交互式命令行，支持 `connect/login/query/add/update/delete` 等命令，用于调试和演示
- **Bridge 模式** (`dcn_client.exe --bridge`)：通过 stdin/stdout 以 TSV 行协议与父进程通信，作为 Flutter GUI 的后端子进程

### 7. Flutter GUI 客户端

- 基于 Material Design 3 的跨平台 UI
- 通过 `CppBridge` Dart 类启动 C++ 客户端子进程
- 支持连接配置、管理员登录、三种查询方式、课程增删改
- 右侧实时显示 Bridge 通信日志

---

## 构建与运行

### 前置条件

- CMake ≥ 3.20
- C++17 编译器
- Windows（Winsock2）或 MinGW 交叉编译
- Flutter SDK（GUI 客户端）

### 初始化子模块

```bash
git submodule update --init --recursive
```

### 生成 32 字节密钥

```bash
mkdir -p key
openssl rand -out key/app.key 32
```

将 `app.key` 放在服务端和客户端的**工作目录**下。

### CMake 构建

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

构建产物：

- `build/programs/Server/dcn_server.exe` — 服务端
- `build/programs/Client/dcn_client.exe` — 客户端

### 启动服务端

```bash
cd build
./programs/Server/dcn_server.exe
```

首次启动自动创建数据库并注入默认管理员 `admin/admin123`。

### 启动客户端

```bash
# REPL 交互模式
./build/programs/Client/dcn_client.exe

# 在 REPL 中使用:
#   connect 127.0.0.1 9001
#   login admin admin123
#   query_code CS101
#   add CS101 "Intro CS" A "Dr.Smith" Mon 90 B201
#   quit
```

### 启动 Flutter GUI

```bash
cd dcn_flutter_client
flutter run -d windows
```

---

## 设计决策

| 决策                          | 理由                                        |
| ----------------------------- | ------------------------------------------- |
| Protobuf 序列化包体           | 跨语言、高效二进制序列化，比 JSON 更紧凑    |
| C 结构体 Header               | 固定 56 字节，无需序列化框架，解析零开销    |
| AES + HMAC (Encrypt-then-MAC) | 先加密再 MAC，防选择密文攻击                |
| 集中式 Dispatcher             | 鉴权/异常处理一处完成，Handler 不重复造轮子 |
| WAL 模式                      | 支持读写并发，多线程环境下吞吐量更高        |
| 显式事务                      | INSERT/UPDATE 涉及多表，事务保证原子性      |
| Bridge 模式                   | C++ 层处理所有网络/加密逻辑，Dart 层仅做 UI |

---

## 协议不变量

1. **方向独占**：C2S 命令不会被服务端发出，反之亦然
2. **有响应必有其 cmd_type**：Handler 永不返回未初始化响应
3. **Dispatcher 永不抛出**：所有异常被捕获为 `CMD_SERVER_ERROR`
4. **集中鉴权**：权限检查只在 Dispatcher 中发生
5. **req_id 唯一**：每包原子递增，便于客户端关联请求/响应
6. **收包失败即断连**：不区分网络错误与协议错误，统一断连

---

## 测试

```bash
cd build
cmake --build . --target dcn_tests
./Tests/dcn_tests
```

测试覆盖：

- 数据库 CRUD 操作
- 协议编解码
- 命令注册与分发
- Socket 收发
- OpenSSL 加解密流程

# How To Build

please remember to init submodule:

- protobuf
- libopenssl

```bash
git submodule update --init --recursive
```

Then you can build the project using CMake:

```bash
mkdir build
cd build
cmake ..
make
```

## Generate 32-bit Key

we need to generate a 32-bit key and put into `DCN1003-Group21-2026/key` folder.

use this command to generate key

```bash
mkdir key
cd key
openssl rand -out app.key 32
```

## How to update version

edit file `config.h.in`.

## About package

we use C struct for package head, Google protobuf for package body

we only use AES to Encryption package body, header will Plaintext Transmission.

**when package send:**

- build up and serialization package body
  - we use function `body.set_xxx()` to put sending part into `google::protobuf::Message&` type

```protobuf
message Payload {
    repeated string json = 1; // 变长数组
}

message MsgBody {
	uint32 cmd_type = 1;	// 命令码
    uint32 req_id = 2;      // uint32_t 命令号
    uint32 timestamp = 3;	// 时间戳
    Payload payload = 4;	// 消息内容
}
```

- Then, we must Encryption Msg.

  - set a secrety key in server / client.
    - use command `openssl rand -out my_secret.key 32`(See step《Generate 32-bit Key》)
  - generate **VI**
  - USE secret_key and VI to calculate MsgBody **HMAC**
  - use **AES** to Encryption MsgBody.
  - AES result will be send by socket_send function.
- generate package header

  - we need:
    - **HMAC** result
    - **VI** result
    - calculate **AES** result length
    - get protocal version
  - put them into `MsgHeader` c struct

```c++
struct MsgHeader
{
    uint32_t version;		// 协议版本号
    uint32_t body_len;		// 加密后有效字段长度
    uint8_t iv[16];			// AES初始向量
    uint8_t mac[32] = {0};	// 消息认证码
};
```

- Send in sequence **header** and **body**.

**Receive Package**

- we receive package header as plaintext, we know it’s how long, so we can verfiyed it use `length()`
  - we got hash \ cmd_type \ protocol version
- then we try to
