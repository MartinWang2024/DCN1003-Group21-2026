# Client Bridge Protocol (stdio TSV)

The DCN1003 course schedule client uses a **two-layer architecture**:

```
┌─────────────────┐  stdio TSV  ┌──────────────────────┐  TCP+AES+HMAC  ┌────────────┐
│ Flutter UI      │ ──────────► │ dcn_client.exe       │ ─────────────► │ dcn_server │
│ (Dart, Win exe) │ ◄────────── │ (Winsock + protocol) │ ◄───────────── │  (.exe)    │
└─────────────────┘             └──────────────────────┘                 └────────────┘
```

`dcn_client.exe` is a complete C++ Winsock client (the assignment deliverable). It can run
standalone as a command-line REPL, or it can act as a Flutter subprocess and expose a
bridge over a stdin/stdout TSV line protocol.

---

## 1. Launch modes

| Mode   | Command line                | Behavior                                                |
|--------|-----------------------------|---------------------------------------------------------|
| REPL   | `dcn_client.exe`            | Interactive terminal with `> ` prompt, for manual demo. |
| Bridge | `dcn_client.exe --bridge`   | Silent stdio mode used as a Flutter subprocess.         |

Both modes share the same verb set and field order; only the prompt/echo behavior differs.

---

## 2. Frame format

- **One message per line**, terminated by `\n` (`\r\n` is also accepted on Windows; the parser trims `\r`).
- **Fields are separated by `\t` (0x09)**.
- **Fields must not contain `\t` or `\n`** (course fields are plain text and stay within the assignment scope).
- Encoding: UTF-8.

---

## 3. Request verbs (Flutter -> C++)

| Verb               | Fields                                                                          | cmd_type                     |
|--------------------|---------------------------------------------------------------------------------|------------------------------|
| `CONNECT`          | `<host>\t<port>`                                                                | -                            |
| `LOGIN`            | `<username>\t<password>`                                                        | CMD_LOGIN_REQ                |
| `LOGOUT`           | -                                                                               | CMD_LOGOUT_REQ               |
| `QUERY_CODE`       | `<code>`                                                                        | CMD_QUERY_BY_CODE_REQ        |
| `QUERY_INSTRUCTOR` | `<instructor>`                                                                  | CMD_QUERY_BY_INSTRUCTOR_REQ  |
| `QUERY_SEMESTER`   | `<semester>`                                                                    | CMD_QUERY_BY_SEMESTER_REQ    |
| `ADD`              | `<code>\t<title>\t<section>\t<instructor>\t<day>\t<duration>\t<classroom>`      | CMD_ADD_REQ                  |
| `UPDATE`           | Same as ADD                                                                     | CMD_UPDATE_REQ               |
| `DELETE`           | `<code>\t<section>`                                                             | CMD_DELETE_REQ               |
| `DISCONNECT`       | -                                                                               | -                            |
| `QUIT`             | -                                                                               | -                            |

`CONNECT` must be issued before any server-bound verb.
`DISCONNECT` closes the socket but keeps the bridge process alive so a new `CONNECT` can follow.
`QUIT` terminates the bridge process.

---

## 4. Response status lines (C++ -> Flutter)

| Status  | Fields                                                                                                                | Meaning                                          |
|---------|-----------------------------------------------------------------------------------------------------------------------|--------------------------------------------------|
| `OK`    | `<message>`                                                                                                           | Success, no structured payload.                  |
| `ERR`   | `<error message>`                                                                                                     | Failure (network / protocol / server rejection). |
| `DATA`  | `<count>\t<c1.code>\t<c1.title>\t<c1.section>\t<c1.instructor>\t<c1.day>\t<c1.duration>\t<c1.classroom>\t<c2.code>...` | Query result.                                    |
| `READY` | -                                                                                                                     | Bridge startup handshake, emitted once at boot.  |

**Total DATA fields** = `1 + 7 * count`. When `count = 0` only the single field `DATA\t0\n` is emitted.

---

## 5. Sample sequences

### Successful login + query
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

### Error / reconnect
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

## 6. Implementation index

| File                                          | Responsibility                                       |
|-----------------------------------------------|------------------------------------------------------|
| `programs/Client/src/client_main.cpp`         | REPL + bridge dual entry; command dispatch.          |
| `programs/Client/src/cmdreg.cpp`              | 8 verbs wrapping Package_send + Package_receive.     |
| `programs/Client/src/connect_to.cpp`          | TCP connect factory (mirror of server/listener.cpp). |
| `flutter_client/lib/bridge/cpp_bridge.dart`   | Dart-side subprocess wrapper.                        |
