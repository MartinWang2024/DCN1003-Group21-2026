# Client Bridge (stdio TSV)

Two-layer client:

```
+-----------------+  stdio TSV  +----------------------+  TCP+AES+HMAC  +------------+
| Flutter UI      | ----------> | dcn_client.exe       | -------------> | dcn_server |
| (Dart, Win exe) | <---------- | (Winsock + protocol) | <------------- |  (.exe)    |
+-----------------+             +----------------------+                +------------+
```

`dcn_client.exe` is a complete C++ Winsock client. It runs standalone as a REPL, or as a
Flutter subprocess exposing a stdin/stdout TSV bridge.

## Launch modes

| Mode   | Command line                | Behavior                                                |
|--------|-----------------------------|---------------------------------------------------------|
| REPL   | `dcn_client.exe`            | Interactive terminal with `> ` prompt, for manual demo. |
| Bridge | `dcn_client.exe --bridge`   | Silent stdio mode used as a Flutter subprocess.         |

Both modes share the verb set and field order; only prompt/echo differs.

## Frame format

- One message per line, terminated by `\n` (parser also trims `\r`).
- Fields separated by `\t` (0x09).
- Fields must not contain `\t` or `\n`.
- Encoding: UTF-8.

## Requests (Flutter -> C++)

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

`CONNECT` precedes any server-bound verb. `DISCONNECT` closes the socket but keeps the
bridge alive for a new `CONNECT`. `QUIT` exits the bridge process.

## Responses (C++ -> Flutter)

| Status  | Fields                                                                                                                | Meaning                                          |
|---------|-----------------------------------------------------------------------------------------------------------------------|--------------------------------------------------|
| `OK`    | `<message>`                                                                                                           | Success, no structured payload.                  |
| `ERR`   | `<error message>`                                                                                                     | Failure (network / protocol / server rejection). |
| `DATA`  | `<count>\t<c1.code>\t<c1.title>\t<c1.section>\t<c1.instructor>\t<c1.day>\t<c1.duration>\t<c1.classroom>\t<c2.code>...` | Query result.                                    |
| `READY` | -                                                                                                                     | Bridge startup handshake, emitted once at boot.  |

Total DATA fields = `1 + 7 * count`. When `count = 0`, the frame is `DATA\t0\n`.

## Samples

Login + query:
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

Error and reconnect:
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

## Implementation index

| File                                          | Responsibility                                       |
|-----------------------------------------------|------------------------------------------------------|
| `programs/Client/src/client_main.cpp`         | REPL + bridge dual entry; command dispatch.          |
| `programs/Client/src/cmdreg.cpp`              | 8 verbs wrapping Package_send + Package_receive.     |
| `programs/Client/src/connect_to.cpp`          | TCP connect factory (mirror of server/listener.cpp). |
| `flutter_client/lib/bridge/cpp_bridge.dart`   | Dart-side subprocess wrapper.                        |
