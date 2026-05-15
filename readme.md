# DCN1003 Course Schedule System - Group 21

A computer-networking course project: a client/server course schedule system with a custom
application-layer protocol, encrypted transport, persistent storage, and a cross-platform
GUI client.

---

## System architecture

```
┌──────────────────────┐                         ┌──────────────────────┐
│   Flutter GUI client │  stdio (TSV protocol)   │   Course DB (SQLite) │
│  (dcn_flutter_client)│────────┐       ┌────────│   data/DCN.db        │
└──────────────────────┘        │       │        │                      │
                                v       v        └──────────────────────┘
┌──────────────────────┐  ┌──────────────────┐            ^
│   C++ REPL client    │  │   C++ Server     │            │
│   (dcn_client)       │  │   (dcn_server)   │            │
└──────────┬───────────┘  └────────┬─────────┘            │
           │                       │                      │
           │  TCP + AES + HMAC     │                      │
           └───────────────────────┘                      │
                    ^                                     │
                    │        ┌────────────────────────────┘
                    │        │
              ┌─────┴────────┴─────┐
              │   Shared lib        │
              │      (Driver)       │
              │  - protocol codec   │
              │  - AES / HMAC       │
              │  - command dispatch │
              │  - DB access        │
              └─────────────────────┘
```

### Project layout

| Directory             | Description                                                              |
| --------------------- | ------------------------------------------------------------------------ |
| `driver/`             | Shared library: protocol codec, AES/HMAC, command dispatcher, DB repos.  |
| `programs/Server/`    | TCP server: per-connection threads, command registration, business logic.|
| `programs/Client/`    | C++ client with REPL mode and Bridge subprocess mode.                    |
| `dcn_flutter_client/` | Flutter GUI that drives the C++ client over stdio.                       |
| `third_party/`        | Vendored dependencies: protobuf, libopenssl.                             |
| `data/`               | SQLite database file.                                                    |
| `tests/`              | Unit tests.                                                              |
| `documents/`          | Design documents.                                                        |

---

## Features

### 1. Custom application-layer protocol

Hybrid frame format built on **protobuf + a fixed C struct**:

```
+-------------------+  <- MsgHeader (56 bytes, plaintext)
| version   (4B)    |  Protocol version
| body_len  (4B)    |  Ciphertext length
| iv        (16B)   |  AES-256-CBC initialization vector
| mac       (32B)   |  HMAC-SHA256 message authentication code
+-------------------+
|                   |  <- Encrypted body (variable length)
| AES-256-CBC text  |
| (Protobuf MsgBody)|
+-------------------+
```

Design notes:

- Header is sent in plaintext and includes the protocol version for compatibility checks.
- Body is encrypted with **AES-256-CBC**; a fresh IV is generated per frame.
- **HMAC-SHA256** is computed over the plaintext MsgBody so the receiver can detect tampering after decryption.
- The shared key is loaded from `app.key` (32 bytes); the same file is used by both client and server.
- Maximum body size is 16 MiB (`MAX_BODY`).

Protobuf schema (`message.proto`):

```protobuf
message Payload {
    repeated bytes json = 1;  // Variable-length positional argument array.
}
message MsgBody {
    uint32 cmd_type  = 1;     // Command code.
    uint32 req_id    = 2;     // Request id (atomic counter).
    uint32 timestamp = 3;     // Timestamp.
    Payload payload  = 4;     // Business payload.
}
```

### 2. Command dispatch

Layered architecture; the dispatcher lives in the `driver` shared library.

```
L4  Connection loop (per-thread)
    while(alive) { recv -> dispatch -> send }

L3  Dispatcher
    - Routes cmd_type to a registered handler.
    - Centralized authorization (ADMIN / STUDENT).
    - Catches exceptions and returns CMD_SERVER_ERROR so the loop never crashes.

L2  Handlers
    - Parse payload fields, call the database, build the response.

L1  Data access layer
    - CourseRepository / AdministratorRepository
```

Supported commands:

| Command               | Role    | Description                                               |
| --------------------- | ------- | --------------------------------------------------------- |
| `LOGIN`               | Student | Admin authentication; on success the session is upgraded. |
| `LOGOUT`              | Student | Logout; session is downgraded back to STUDENT.            |
| `QUERY_BY_CODE`       | Student | Exact-match query by course code.                         |
| `QUERY_BY_INSTRUCTOR` | Student | Fuzzy query by instructor name.                           |
| `QUERY_BY_SEMESTER`   | Student | Query by semester (reserved).                             |
| `ADD`                 | Admin   | Insert a course and its schedule (transactional).         |
| `UPDATE`              | Admin   | Update course information (transactional).                |
| `DELETE`              | Admin   | Cascade-delete a course and its schedule rows.            |

Authorization runs only inside `Dispatcher::dispatch()`; handlers never repeat the check.

### 3. Secure transport

| Mechanism          | Algorithm     | Notes                                                              |
| ------------------ | ------------- | ------------------------------------------------------------------ |
| Symmetric encrypt  | AES-256-CBC   | Body encryption with a per-frame random IV.                        |
| Message auth       | HMAC-SHA256   | MAC over the plaintext serialization to prevent tampering / replay.|
| Access control     | Session role  | STUDENT (read-only) / ADMIN (CRUD).                                |
| Protocol check     | Version field | Receiver rejects frames with an incompatible version.              |

Pipeline:

1. **Send**: serialize MsgBody -> compute HMAC -> generate random IV -> AES encrypt -> send header then ciphertext.
2. **Receive**: read header -> verify version -> read ciphertext -> AES decrypt -> verify HMAC -> protobuf decode.

### 4. Database

The system uses **SQLite** with WAL mode and foreign keys enabled.

**`courses`** (course basics):

| Column       | Type         | Constraints |
| ------------ | ------------ | ----------- |
| `code`       | VARCHAR(32)  | PK, NOT NULL |
| `section`    | VARCHAR(32)  | PK, NOT NULL |
| `title`      | VARCHAR(128) | NOT NULL    |
| `instructor` | VARCHAR(64)  | NOT NULL    |

**`schedules`** (timeslots):

| Column        | Type        | Constraints      |
| ------------- | ----------- | ---------------- |
| `course_code` | VARCHAR(32) | PK, FK -> courses |
| `section`     | VARCHAR(32) | PK, FK -> courses |
| `day`         | DATE        | PK               |
| `duration`    | VARCHAR(32) | PK               |
| `semester`    | VARCHAR(16) | PK               |
| `classroom`   | VARCHAR(64) | NOT NULL         |

`ON DELETE CASCADE` keeps `schedules` consistent. All write paths run inside an explicit
transaction (BEGIN/COMMIT/ROLLBACK).

**`administrators`**:

| Column     | Type         | Constraints |
| ---------- | ------------ | ----------- |
| `username` | VARCHAR(64)  | PRIMARY KEY |
| `password` | VARCHAR(255) | NOT NULL    |

Passwords are stored as PBKDF2-HMAC-SHA256 hashes (see `driver/src/password_hash.cpp`).

### 5. Multi-threaded server

- Winsock2 TCP, listening on port 9001.
- Each accepted connection runs on its own detached `std::thread`.
- Each thread runs `handle_client`: receive -> dispatch -> respond, until the peer closes.
- The database and dispatcher are shared across threads via `std::ref`; thread safety relies on SQLite `FULLMUTEX`.

### 6. Dual-mode C++ client

- **REPL mode** (`dcn_client.exe`): an interactive shell with `connect / login / query / add / update / delete / quit`, used for demos and debugging.
- **Bridge mode** (`dcn_client.exe --bridge`): silent stdin/stdout TSV line protocol, intended to run as a Flutter subprocess.

### 7. Flutter GUI

- Cross-platform UI built on Material Design 3.
- The `CppBridge` Dart class spawns and drives `dcn_client.exe`.
- Supports connection setup, admin login, the three query modes, and course CRUD.
- A live bridge log is rendered on the right-hand side.

---

## Build and run

### Prerequisites

- CMake >= 3.20
- A C++17 compiler
- Windows (Winsock2) or MinGW cross-compile
- Flutter SDK (for the GUI client)

### Initialize submodules

```bash
git submodule update --init --recursive
```

### Generate the 32-byte key

```bash
mkdir -p key
openssl rand -out key/app.key 32
```

`app.key` must sit in the **working directory** of both the server and the client.

### CMake build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

Build outputs:

- `build/programs/Server/dcn_server.exe` - the server.
- `build/programs/Client/dcn_client.exe` - the C++ client.

### Start the server

```bash
cd build
./programs/Server/dcn_server.exe
```

On first launch the server creates the database and seeds a default administrator
`admin / admin123`.

### Start the C++ client

```bash
# REPL mode
./build/programs/Client/dcn_client.exe

# Inside the REPL:
#   connect 127.0.0.1 9001
#   login admin admin123
#   query_code CS101
#   add CS101 "Intro CS" A "Dr.Smith" Mon 90 B201
#   quit
```

### Start the Flutter GUI

```bash
cd dcn_flutter_client
flutter run -d windows
```

---

## Design choices

| Choice                        | Rationale                                                                   |
| ----------------------------- | --------------------------------------------------------------------------- |
| Protobuf body                 | Cross-language, compact binary serialization (smaller than JSON).           |
| C-struct header               | Fixed 56 bytes, no codec overhead, parses with a single memcpy.             |
| AES + HMAC (Encrypt-then-MAC) | Encrypt first, then authenticate; protects against chosen-ciphertext attacks. |
| Centralized dispatcher        | Authorization and exception handling live in one place.                     |
| WAL mode                      | Better concurrent throughput when reads and writes mix.                     |
| Explicit transactions         | INSERT/UPDATE touches multiple tables; transactions keep them atomic.       |
| Bridge mode                   | All networking and crypto stay in C++; Dart only owns the UI.               |

---

## Protocol invariants

1. **Direction-exclusive**: C2S commands are never emitted by the server, and vice versa.
2. **Every response has a `cmd_type`**: handlers never return an uninitialized response.
3. **The dispatcher never throws**: every exception is caught and converted to `CMD_SERVER_ERROR`.
4. **Centralized authorization**: permission checks happen only in the dispatcher.
5. **`req_id` is unique**: incremented atomically per frame so the client can correlate request/response.
6. **Receive failure tears down the connection**: network errors and protocol errors are not differentiated; the connection is closed unconditionally.

---

## Tests

```bash
cd build
cmake --build . --target dcn_tests
./Tests/dcn_tests
```

Coverage:

- Database CRUD.
- Protocol codec.
- Command registration and dispatch.
- Socket send/receive.
- OpenSSL encrypt/decrypt pipeline.
