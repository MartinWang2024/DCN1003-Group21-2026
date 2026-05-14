# DCN1003 应用层协议规范

> 课程表查询系统的命令-响应协议，基于 protobuf + AES + HMAC 的可靠传输。

---

## 数据帧结构

### 帧布局（线上字节序）

```
+---------------------------+  ← MsgHeader_t (固定 56 字节)
|  version    (4B)          |
|  body_len   (4B)          |
|  iv         (16B)         |
|  mac        (32B)         |
+---------------------------+
|                           |  ← Encrypted Body (变长 = body_len)
|  AES-256-CBC(MsgBody)     |
|     with iv               |
|                           |
+---------------------------+
```

- **version**: 协议版本号，与 `APP_VERSION` 编译期常量校验。
- **body_len**: AES 加密后密文字节数，最大 `MAX_BODY = 16 MiB`。
- **iv**: AES-256-CBC 初始向量（每包随机生成）。
- **mac**: HMAC-SHA256 校验码，作用于明文 MsgBody 序列化结果。
- **MsgBody**: protobuf 序列化后再 AES 加密。

### protobuf 定义（`message.proto`）

```proto
message Payload {
    repeated bytes json = 1;
}

message MsgBody {
    uint32 cmd_type  = 1;
    uint32 req_id    = 2;
    uint32 timestamp = 3;
    Payload payload  = 4;
}
```

### 收发流程

| 阶段 | 发送端 | 接收端 |
|---|---|---|
| ① | 序列化 MsgBody → `body_serialized` | 接收 56B Header |
| ② | HMAC(body_serialized) → `header.mac` | 校验 version |
| ③ | 生成随机 IV → `header.iv` | 接收 `body_len` 字节密文 |
| ④ | AES 加密 `body_serialized` → `ciphertext` | AES 解密 → `plaintext` |
| ⑤ | `header.body_len = ciphertext.size()` | HMAC(plaintext) 与 `header.mac` 比对 |
| ⑥ | 发送 Header 后发送 ciphertext | 反序列化 plaintext → MsgBody |

---

## 命令码（cmd_type）

### 编码方案

```c++
constexpr uint32_t C2S = 0x00000000;  // Client → Server
constexpr uint32_t S2C = 0xF0000000;  // Server → Client
```

判定方向：
```c++
constexpr bool is_c2s(uint32_t c) { return (c & 0x80000000) == 0; }
constexpr bool is_s2c(uint32_t c) { return (c & 0x80000000) != 0; }
```

### 完整命令表

| Mnemonic | 值 | 方向 | 权限 | 说明 |
|---|---|---|---|---|
| **认证** | | | | |
| `CMD_LOGIN_REQ`               | `0x00000001` | C2S | STUDENT | 登录请求 |
| `CMD_LOGIN_RESP`              | `0xF0000002` | S2C | -       | 登录响应 |
| `CMD_LOGOUT_REQ`              | `0x00000003` | C2S | STUDENT | 登出请求 |
| **查询** | | | | |
| `CMD_QUERY_BY_CODE_REQ`       | `0x00000101` | C2S | STUDENT | 按课程编码查询 |
| `CMD_QUERY_BY_INSTRUCTOR_REQ` | `0x00000102` | C2S | STUDENT | 按教师姓名查询 |
| `CMD_QUERY_BY_SEMESTER_REQ`   | `0x00000103` | C2S | STUDENT | 按学期查询 |
| `CMD_QUERY_RESP`              | `0xF0000180` | S2C | -       | 查询结果响应 |
| **管理（需 ADMIN 权限）** | | | | |
| `CMD_ADD_REQ`                 | `0x00000201` | C2S | ADMIN   | 新增课程 |
| `CMD_UPDATE_REQ`              | `0x00000202` | C2S | ADMIN   | 更新课程 |
| `CMD_DELETE_REQ`              | `0x00000203` | C2S | ADMIN   | 删除课程 |
| `CMD_ADMIN_RESP`              | `0xF0000280` | S2C | -       | 管理操作通用响应 |
| **通用响应** | | | | |
| `CMD_OK`                      | `0xF000FF00` | S2C | -       | 通用成功 |
| `CMD_ERROR`                   | `0xF000FFFF` | S2C | -       | 业务错误（参数/凭据等） |
| `CMD_PERMISSION_ERR`          | `0xF000FFFE` | S2C | -       | 权限不足 |
| `CMD_SERVER_ERROR`            | `0xF000FFFD` | S2C | -       | 服务器内部错误 |

---

## Payload 编码约定

### 通用规则

- `Payload.json` 是 `repeated bytes`，按位置传递字段（不是 JSON 文本，是位置参数数组）。
- 字段从索引 `0` 开始；缺失的尾部字段视为空串。
- 二进制安全：避免 UTF-8 校验，可装任意字节。

### 响应序列化格式（查询命令使用）

查询响应的 `payload` 用 ASCII 控制字符序列化课程列表：

```
<count><RS><course1><RS><course2>...
```

其中每条记录：
```
<code><US><title><US><section><US><instructor><US><day><US><duration><US><classroom>
```

- `RS = 0x1E`（Record Separator）
- `US = 0x1F`（Unit Separator）

选用控制字符避免与课程字段（中英文/数字/空格）冲突。

---

## 命令详情

### LOGIN — 登录

**请求** `CMD_LOGIN_REQ` (C2S)

| Index | 字段 | 类型 | 说明 |
|---|---|---|---|
| `json[0]` | username | bytes | 管理员用户名 |
| `json[1]` | password | bytes | 明文密码 |

**响应**

| 场景 | cmd_type | payload |
|---|---|---|
| 成功 | `CMD_LOGIN_RESP` | `"OK"` |
| 字段缺失 | `CMD_ERROR` | `"Login payload requires username and password"` |
| 凭据为空 | `CMD_ERROR` | `"Username or password cannot be empty"` |
| 凭据错误 | `CMD_ERROR` | `"Invalid username or password"` |

**副作用**：成功时 `Session.role` 升级为 `ADMIN`，`Session.name` 设为 username。

---

### LOGOUT — 登出

**请求** `CMD_LOGOUT_REQ` (C2S)

无字段。

**响应**

| 场景 | cmd_type | payload |
|---|---|---|
| 成功 | `CMD_OK` | `"OK"` |

**副作用**：`Session.role` 降级为 `STUDENT`，`Session.name` 清空。**连接保留**，客户端可重新登录。

---

### QUERY_BY_CODE — 按课程编码查询

**请求** `CMD_QUERY_BY_CODE_REQ` (C2S)

| Index | 字段 | 类型 | 说明 |
|---|---|---|---|
| `json[0]` | code | bytes | 课程编码，如 `"CS101"` |

**响应**

| 场景 | cmd_type | payload |
|---|---|---|
| 命中 N 条 | `CMD_QUERY_RESP` | `<N><RS>...` 序列化课程列表 |
| 无命中 | `CMD_QUERY_RESP` | `"0"` |
| code 为空 | `CMD_ERROR` | `"Course code required"` |

---

### QUERY_BY_INSTRUCTOR — 按教师查询

**请求** `CMD_QUERY_BY_INSTRUCTOR_REQ` (C2S)

| Index | 字段 | 类型 | 说明 |
|---|---|---|---|
| `json[0]` | instructor | bytes | 教师姓名，如 `"Dr. Smith"` |

**响应**：空值时返回 `CMD_ERROR` + `"Instructor name required"`。

---

### QUERY_BY_SEMESTER — 按学期查询

**请求** `CMD_QUERY_BY_SEMESTER_REQ` (C2S)

| Index | 字段 | 类型 | 说明 |
|---|---|---|---|
| `json[0]` | semester | bytes | 学期标识 |

**响应**：当前实现为 **stub**。

| 场景 | cmd_type | payload |
|---|---|---|
| 任何输入 | `CMD_ERROR` | `"Query by semester not supported in current schema"` |

> **未实现原因**：`Course` schema 当前无 `semester` 字段，`CourseRepository` 无对应查询接口。需扩展数据库后补齐。

---

### ADD — 新增课程（admin）

**请求** `CMD_ADD_REQ` (C2S, role=ADMIN)

| Index | 字段 | 类型 | 必填 |
|---|---|---|---|
| `json[0]` | code       | bytes | Y |
| `json[1]` | title      | bytes | Y |
| `json[2]` | section    | bytes | Y |
| `json[3]` | instructor | bytes | Y |
| `json[4]` | day        | bytes | Y |
| `json[5]` | duration   | bytes | Y |
| `json[6]` | classroom  | bytes | Y |

> 主键为 `(code, section)` 复合键。重复写入会覆盖。

**响应**

| 场景 | cmd_type | payload |
|---|---|---|
| 成功 | `CMD_ADMIN_RESP` | `"OK"` |
| 字段不足 7 个 | `CMD_ERROR` | `"Add requires 7 fields..."` |
| code/section 为空 | `CMD_ERROR` | `"Course code and section cannot be empty"` |
| DB 写入失败 | `CMD_SERVER_ERROR` | `"Failed to insert course: <details>"` |
| 权限不足 | `CMD_PERMISSION_ERR` | `"Insufficient permissions"`（dispatcher 拦截） |

---

### UPDATE — 更新课程（admin）

**请求** `CMD_UPDATE_REQ` (C2S, role=ADMIN)

字段表与 ADD 相同。`(code, section)` 用于定位记录，其余 5 个字段为新值。

**响应**

| 场景 | cmd_type | payload |
|---|---|---|
| 成功 | `CMD_ADMIN_RESP` | `"OK"` |
| 字段不足 7 个 | `CMD_ERROR` | `"Update requires 7 fields"` |
| code/section 为空 | `CMD_ERROR` | `"Course code and section cannot be empty"` |
| DB 更新失败 | `CMD_SERVER_ERROR` | `"Failed to update course: <details>"` |
| 权限不足 | `CMD_PERMISSION_ERR` | dispatcher 拦截 |

> 注意：当前 schema 下，"记录不存在"不会显式区分（取决于 `CourseRepository::update` 实现是否检查 affected rows）。

---

### DELETE — 删除课程（admin）

**请求** `CMD_DELETE_REQ` (C2S, role=ADMIN)

| Index | 字段 | 类型 | 说明 |
|---|---|---|---|
| `json[0]` | code    | bytes | 课程编码 |
| `json[1]` | section | bytes | 班级 |

**响应**

| 场景 | cmd_type | payload |
|---|---|---|
| 成功 | `CMD_ADMIN_RESP` | `"OK"` |
| 字段不足 2 个 | `CMD_ERROR` | `"Delete requires code and section"` |
| 任一字段为空 | `CMD_ERROR` | `"Course code and section cannot be empty"` |
| DB 删除失败 | `CMD_SERVER_ERROR` | `"Failed to delete course: <details>"` |
| 权限不足 | `CMD_PERMISSION_ERR` | dispatcher 拦截 |

---

## 协议不变量

1. 方向独占：C2S 命令永远不会被服务端发出，反之亦然。可用 `is_c2s/is_s2c` 校验。
2. 响应必有 cmd_type：handler 永不返回未初始化的 `Response_t`。
3. dispatcher 永不抛出：所有未知异常被捕获为 `CMD_SERVER_ERROR`。
4. 集中鉴权：权限检查只发生在 `Dispatcher::dispatch`，handler 内部假设权限已通过。
5. req_id 唯一：每包递增（原子计数器），便于客户端关联请求/响应。
6. 任何 receive 失败 = 关连接：不区分网络错误与协议错误，统一断连，避免攻击放大面。

---

## 时序示例

### 学生查询课程

```
Client                           Server
  | ── CMD_QUERY_BY_CODE_REQ ──→  |
  |   json=["CS101"]              |  (Session.role=STUDENT, OK)
  |                               |  CourseRepo.search_by_course_code
  | ←── CMD_QUERY_RESP ────────── |
  |   payload="2<RS>CS101<US>..."  |
```

### 管理员登录后增删课程

```
Client                           Server
  | ── CMD_LOGIN_REQ ──────────→ |
  |   json=["alice","secret123"] |
  |                              |  AdminRepo.verify_login
  | ←── CMD_LOGIN_RESP ───────── |  Session.role=ADMIN
  |
  | ── CMD_ADD_REQ ────────────→ |
  |   json=[7 fields]            |
  | ←── CMD_ADMIN_RESP ──────── |  payload="OK"
  |
  | ── CMD_LOGOUT_REQ ─────────→ |
  | ←── CMD_OK ───────────────── |  Session 降级
```

### 学生越权

```
Client                           Server
  | ── CMD_DELETE_REQ ─────────→ |  (Session.role=STUDENT)
  |   json=["CS101","A"]         |  Dispatcher 鉴权失败
  | ←── CMD_PERMISSION_ERR ───── |  payload="Insufficient permissions"
                                    (DB 未被触碰)
```

---

## 实现位置索引

| 内容 | 文件 |
|---|---|
| 包头/包体结构 | `Driver/include/protocol.h` |
| 收发实现 | `Driver/src/protocol.cpp` |
| 命令枚举 | `Driver/include/cmd_type.h` |
| Dispatcher | `Driver/src/CmdHandler.cpp` |
| Server handlers | `programs/Server/src/cmdreg.cpp` |
| Server 注册入口 | `register_all_server()` in `cmdreg.cpp` |
| 单元测试 | `Tests/test_cmdreg.cpp`、`Tests/test_protocol.cpp` |
