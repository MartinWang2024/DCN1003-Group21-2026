# Client Bridge Protocol (stdio TSV)

DCN1003 课程表客户端的 **双层架构**：

```
┌─────────────────┐  stdio TSV  ┌──────────────────────┐  TCP+AES+HMAC  ┌────────────┐
│ Flutter UI      │ ──────────► │ dcn_client.exe       │ ─────────────► │ dcn_server │
│ (Dart, Win exe) │ ◄────────── │ (Winsock + protocol) │ ◄───────────── │  (.exe)    │
└─────────────────┘             └──────────────────────┘                 └────────────┘
```

`dcn_client.exe` 是完整的 C++ Winsock 客户端 (作业可交付物)，可独立用命令行 REPL 运行；
也可作为 Flutter 子进程，通过 stdin/stdout TSV 行协议提供桥接服务。

---

## 1. 启动模式

| 模式      | 命令行                       | 行为                                                     |
|-----------|------------------------------|----------------------------------------------------------|
| REPL      | `dcn_client.exe`             | 交互式终端, 提示符 `> `, 用于人工演示                    |
| Bridge    | `dcn_client.exe --bridge`    | 静默 stdio 模式, 用于 Flutter 子进程                     |

REPL 与 Bridge 模式共享相同的 verb 集与字段顺序, 仅 prompt/echo 差异。

---

## 2. 帧格式

- **每条消息一行**, 以 `\n` 结束 (Windows 上 `\r\n` 也可接受, parser 会 trim `\r`)
- **字段以 `\t` (0x09) 分隔**
- **字段内不允许出现 `\t` 或 `\n`** (课程字段是普通文本, 作业范围内安全)
- 编码 UTF-8

---

## 3. Request Verbs (Flutter → C++)

| Verb               | 字段                                                              | 对应 cmd_type            |
|--------------------|-------------------------------------------------------------------|--------------------------|
| `CONNECT`          | `<host>\t<port>`                                                  | -                        |
| `LOGIN`            | `<username>\t<password>`                                          | CMD_LOGIN_REQ            |
| `LOGOUT`           | -                                                                 | CMD_LOGOUT_REQ           |
| `QUERY_CODE`       | `<code>`                                                          | CMD_QUERY_BY_CODE_REQ    |
| `QUERY_INSTRUCTOR` | `<instructor>`                                                    | CMD_QUERY_BY_INSTRUCTOR_REQ |
| `QUERY_SEMESTER`   | `<semester>`                                                      | CMD_QUERY_BY_SEMESTER_REQ |
| `ADD`              | `<code>\t<title>\t<section>\t<instructor>\t<day>\t<duration>\t<classroom>` | CMD_ADD_REQ      |
| `UPDATE`           | 同 ADD                                                            | CMD_UPDATE_REQ           |
| `DELETE`           | `<code>\t<section>`                                               | CMD_DELETE_REQ           |
| `DISCONNECT`       | -                                                                 | -                        |
| `QUIT`             | -                                                                 | -                        |

`CONNECT` 必须在任何 server-bound verb 之前调用。
`DISCONNECT` 关闭 socket 但保持 bridge 进程存活, 可再次 `CONNECT`。
`QUIT` 关闭 bridge 进程。

---

## 4. Response Status Lines (C++ → Flutter)

| Status   | 字段                                                              | 含义                              |
|----------|-------------------------------------------------------------------|-----------------------------------|
| `OK`     | `<message>`                                                       | 成功, 无结构化数据                |
| `ERR`    | `<error message>`                                                 | 失败 (网络/协议/服务端拒绝)        |
| `DATA`   | `<count>\t<c1.code>\t<c1.title>\t<c1.section>\t<c1.instructor>\t<c1.day>\t<c1.duration>\t<c1.classroom>\t<c2.code>...` | 查询结果 |
| `READY`  | -                                                                 | bridge 启动握手, 仅在进程启动时输出 |

**DATA 字段总数** = `1 + 7 * count`. 当 `count = 0` 时仅有一个字段 (`DATA\t0\n`).

---

## 5. 时序示例

### 成功 login + query
```
< READY
> CONNECT	127.0.0.1	9001
< OK	connected
> LOGIN	alice	secret123
< OK	logged in
> QUERY_CODE	CS101
< DATA	1	CS101	Intro CS	01	Dr.Smith	Mon	90	A101
> QUIT
(bridge process exits)
```

### 错误 / 重连
```
> CONNECT	127.0.0.1	9001
< OK	connected
> LOGIN	alice	wrongpass
< ERR	Invalid username or password
> DISCONNECT
< OK	disconnected
> CONNECT	127.0.0.1	9001
< OK	connected
```

---

## 6. 实现索引

| 文件                                          | 职责                                |
|-----------------------------------------------|-------------------------------------|
| `programs/Client/src/client_main.cpp`         | REPL + bridge 双入口, 命令分发      |
| `programs/Client/src/cmdreg.cpp`              | 8 个 verb -> Package_send + Package_receive 包装 |
| `programs/Client/src/connect_to.cpp`          | TCP 连接工厂 (对称 server/listener.cpp) |
| `flutter_client/lib/bridge/cpp_bridge.dart`   | Dart 端子进程封装                   |
