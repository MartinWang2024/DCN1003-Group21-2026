# DCN1003 Course Schedule System - Group 21

Client/server course schedule system with a custom application-layer protocol,
encrypted transport, SQLite storage, and a Flutter GUI driving a C++ Winsock client.

## Layout

```
+----------------------+                         +----------------------+
|   Flutter GUI client |  stdio (TSV protocol)   |   Course DB (SQLite) |
|  (dcn_flutter_client)|--------+       +--------|   data/DCN.db        |
+----------------------+        |       |        +----------------------+
                                v       v                  ^
+----------------------+  +------------------+             |
|   C++ REPL client    |  |   C++ Server     |             |
|   (dcn_client)       |  |   (dcn_server)   |             |
+----------+-----------+  +--------+---------+             |
           |                       |                       |
           |  TCP + AES + HMAC     |                       |
           +-----------------------+                       |
                       ^                                   |
                       |        +--------------------------+
                       |        |
                 +-----+--------+-----+
                 |   Shared lib       |
                 |     (Driver)       |
                 |  protocol codec    |
                 |  AES / HMAC        |
                 |  dispatcher        |
                 |  DB repositories   |
                 +--------------------+
```

| Directory             | Description                                                          |
|-----------------------|----------------------------------------------------------------------|
| `driver/`             | Shared lib: codec, AES/HMAC, dispatcher, repositories.               |
| `programs/Server/`    | TCP server, per-connection threads, command registration.            |
| `programs/Client/`    | C++ client: REPL and Bridge subprocess modes.                        |
| `dcn_flutter_client/` | Flutter GUI driving the C++ client over stdio.                       |
| `third_party/`        | Vendored deps: protobuf, openssl.                                    |
| `data/`               | SQLite database file.                                                |
| `tests/`              | Unit tests.                                                          |
| `documents/`          | Design docs.                                                         |

## Documents

- [`documents/protocol_define.md`](documents/protocol_define.md) - wire frame, cmd_type table, per-command payload.
- [`documents/architecture.md`](documents/architecture.md) - dispatcher pipeline, schema.
- [`documents/client_bridge.md`](documents/client_bridge.md) - Flutter <-> C++ stdio TSV protocol.

## Frame at a glance

```
+-------------------+  <- MsgHeader (56B, plaintext)
| version    4B     |
| body_len   4B     |
| iv        16B     |  AES-256-CBC IV (random per frame)
| mac       32B     |  HMAC-SHA256 over plaintext MsgBody
+-------------------+
| AES-256-CBC body  |  protobuf MsgBody, up to MAX_BODY = 16 MiB
+-------------------+
```

Encrypt-then-MAC; shared 32-byte key in `app.key`; receiver rejects mismatched version.
See `protocol_define.md` for the full cmd_type table.

## Build

Prerequisites: CMake >= 3.20, C++17, Windows (Winsock2) or MinGW, Flutter SDK.

```bash
git submodule update --init --recursive
openssl rand -out key/app.key 32      # 32-byte shared key, copy next to each binary

mkdir build && cd build
cmake ..
cmake --build .
```

Outputs:
- `build/programs/Server/dcn_server.exe`
- `build/programs/Client/dcn_client.exe`

`app.key` must sit in the working directory of every binary.

## Run

```bash
# Server (default port 9001)
./build/programs/Server/dcn_server.exe [port]

# C++ REPL client
./build/programs/Client/dcn_client.exe
#   connect 127.0.0.1 9001
#   login admin admin123
#   query_code CS101
#   add CS101 "Intro CS" A "Dr.Smith" Mon 90 B201
#   quit

# Flutter GUI
cd dcn_flutter_client && flutter run -d windows
```

First server launch creates `data/DCN.db` and seeds `admin / admin123`.

## Tests

```bash
cmake --build build --target dcn_tests
./build/Tests/dcn_tests
```

Covers DB CRUD, protocol codec, command dispatch, socket I/O, OpenSSL pipeline.

## Protocol invariants

1. C2S commands never originate from the server; S2C never from the client.
2. Every response carries a `cmd_type`; no uninitialized responses.
3. The dispatcher never throws - exceptions become `CMD_SERVER_ERROR`.
4. Authorization is centralized in `Dispatcher::dispatch()`.
5. `req_id` is unique per frame (atomic counter) for request/response correlation.
6. Any receive failure tears down the connection; network and protocol errors are not distinguished.
