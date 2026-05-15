# Application Protocol

Command/response protocol over protobuf + AES-256-CBC + HMAC-SHA256.

## Frame layout

```
+---------------------------+  <- MsgHeader_t (fixed 56 bytes)
|  version    (4B)          |
|  body_len   (4B)          |
|  iv         (16B)         |
|  mac        (32B)         |
+---------------------------+
|                           |  <- Encrypted body (variable, body_len bytes)
|  AES-256-CBC(MsgBody)     |
|     with iv               |
|                           |
+---------------------------+
```

- **version**: protocol version, validated against the compile-time constant `APP_VERSION`.
- **body_len**: ciphertext byte length after AES encryption, capped at `MAX_BODY = 16 MiB`.
- **iv**: AES-256-CBC initialization vector (regenerated randomly for every frame).
- **mac**: HMAC-SHA256 over the plaintext serialized MsgBody.
- **MsgBody**: protobuf-serialized, then AES-encrypted.

Protobuf body (`message.proto`):

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

## Pipeline

| Step | Sender                                              | Receiver                                          |
|------|-----------------------------------------------------|---------------------------------------------------|
| 1    | Serialize MsgBody -> `body_serialized`              | Read 56-byte header.                              |
| 2    | HMAC(body_serialized) -> `header.mac`               | Verify version.                                   |
| 3    | Generate random IV -> `header.iv`                   | Read `body_len` ciphertext bytes.                 |
| 4    | AES encrypt `body_serialized` -> `ciphertext`       | AES decrypt -> `plaintext`.                       |
| 5    | `header.body_len = ciphertext.size()`               | Compare HMAC(plaintext) against `header.mac`.     |
| 6    | Send header, then send ciphertext.                  | Deserialize plaintext -> MsgBody.                 |

---

## cmd_type encoding

```c++
constexpr uint32_t C2S = 0x00000000;  // Client -> Server
constexpr uint32_t S2C = 0xF0000000;  // Server -> Client
```

Direction predicates:
```c++
constexpr bool is_c2s(uint32_t c) { return (c & 0x80000000) == 0; }
constexpr bool is_s2c(uint32_t c) { return (c & 0x80000000) != 0; }
```

## Command table

| Mnemonic                      | Value        | Direction | Role    | Description                                  |
|-------------------------------|--------------|-----------|---------|----------------------------------------------|
| **Authentication**            |              |           |         |                                              |
| `CMD_LOGIN_REQ`               | `0x00000001` | C2S       | STUDENT | Login request.                               |
| `CMD_LOGIN_RESP`              | `0xF0000002` | S2C       | -       | Login response.                              |
| `CMD_LOGOUT_REQ`              | `0x00000003` | C2S       | STUDENT | Logout request.                              |
| **Queries**                   |              |           |         |                                              |
| `CMD_QUERY_BY_CODE_REQ`       | `0x00000101` | C2S       | STUDENT | Query by course code.                        |
| `CMD_QUERY_BY_INSTRUCTOR_REQ` | `0x00000102` | C2S       | STUDENT | Query by instructor name.                    |
| `CMD_QUERY_BY_SEMESTER_REQ`   | `0x00000103` | C2S       | STUDENT | Query by semester.                           |
| `CMD_QUERY_RESP`              | `0xF0000180` | S2C       | -       | Query result response.                       |
| **Admin (requires ADMIN)**    |              |           |         |                                              |
| `CMD_ADD_REQ`                 | `0x00000201` | C2S       | ADMIN   | Add course.                                  |
| `CMD_UPDATE_REQ`              | `0x00000202` | C2S       | ADMIN   | Update course.                               |
| `CMD_DELETE_REQ`              | `0x00000203` | C2S       | ADMIN   | Delete course.                               |
| `CMD_ADMIN_RESP`              | `0xF0000280` | S2C       | -       | Generic admin operation response.            |
| **Generic responses**         |              |           |         |                                              |
| `CMD_OK`                      | `0xF000FF00` | S2C       | -       | Generic success.                             |
| `CMD_ERROR`                   | `0xF000FFFF` | S2C       | -       | Business error (bad arguments, credentials). |
| `CMD_PERMISSION_ERR`          | `0xF000FFFE` | S2C       | -       | Insufficient permissions.                    |
| `CMD_SERVER_ERROR`            | `0xF000FFFD` | S2C       | -       | Server-side internal error.                  |

---

## Payload encoding

`Payload.json` is `repeated bytes` used as a positional argument array (not JSON text).
Fields are zero-indexed; missing trailing fields are empty strings. Binary-safe.

Query responses serialize the course list with ASCII control characters:

```
<count><RS><course1><RS><course2>...
```

Each record:
```
<code><US><title><US><section><US><instructor><US><day><US><duration><US><classroom>
```

`RS = 0x1E`, `US = 0x1F`. These bytes do not appear in course fields.

---

## Command details

### LOGIN

**Request** `CMD_LOGIN_REQ` (C2S)

| Index     | Field     | Type  | Description           |
|-----------|-----------|-------|-----------------------|
| `json[0]` | username  | bytes | Administrator user.   |
| `json[1]` | password  | bytes | Plaintext password.   |

**Response**

| Case             | cmd_type         | payload                                              |
|------------------|------------------|------------------------------------------------------|
| Success          | `CMD_LOGIN_RESP` | `"OK"`                                               |
| Missing fields   | `CMD_ERROR`      | `"Login payload requires username and password"`     |
| Empty fields     | `CMD_ERROR`      | `"Username or password cannot be empty"`             |
| Bad credentials  | `CMD_ERROR`      | `"Invalid username or password"`                     |

**Side effect**: on success, `Session.role` is upgraded to `ADMIN` and `Session.name` is set to the username.

---

### LOGOUT

**Request** `CMD_LOGOUT_REQ` (C2S). No fields.

**Response**

| Case    | cmd_type | payload |
|---------|----------|---------|
| Success | `CMD_OK` | `"OK"`  |

**Side effect**: `Session.role` downgrades to `STUDENT` and `Session.name` is cleared. The connection is preserved so the client can log in again.

---

### QUERY_BY_CODE

**Request** `CMD_QUERY_BY_CODE_REQ` (C2S)

| Index     | Field | Type  | Description                |
|-----------|-------|-------|----------------------------|
| `json[0]` | code  | bytes | Course code, e.g. `"CS101"`. |

**Response**

| Case               | cmd_type         | payload                                                |
|--------------------|------------------|--------------------------------------------------------|
| N rows matched     | `CMD_QUERY_RESP` | `<N><RS>...` serialized course list                    |
| No matches         | `CMD_QUERY_RESP` | `"0"`                                                  |
| Empty code         | `CMD_ERROR`      | `"Course code required"`                               |

---

### QUERY_BY_INSTRUCTOR

**Request** `CMD_QUERY_BY_INSTRUCTOR_REQ` (C2S)

| Index     | Field      | Type  | Description                        |
|-----------|------------|-------|------------------------------------|
| `json[0]` | instructor | bytes | Instructor name, e.g. `"Dr. Smith"`. |

**Response**: empty input returns `CMD_ERROR` + `"Instructor name required"`.

---

### QUERY_BY_SEMESTER

**Request** `CMD_QUERY_BY_SEMESTER_REQ` (C2S)

| Index     | Field    | Type  | Description       |
|-----------|----------|-------|-------------------|
| `json[0]` | semester | bytes | Semester ID.      |

**Response**: currently a **stub**.

| Case      | cmd_type    | payload                                                        |
|-----------|-------------|----------------------------------------------------------------|
| Any input | `CMD_ERROR` | `"Query by semester not supported in current schema"`          |

The current `Course` schema lacks a `semester` field; revisit when the schema is extended.

---

### ADD (admin)

**Request** `CMD_ADD_REQ` (C2S, role=ADMIN)

| Index     | Field      | Type  | Required |
|-----------|------------|-------|----------|
| `json[0]` | code       | bytes | Y        |
| `json[1]` | title      | bytes | Y        |
| `json[2]` | section    | bytes | Y        |
| `json[3]` | instructor | bytes | Y        |
| `json[4]` | day        | bytes | Y        |
| `json[5]` | duration   | bytes | Y        |
| `json[6]` | classroom  | bytes | Y        |

Primary key is the composite `(code, section)`. Re-inserts overwrite.

**Response**

| Case                        | cmd_type             | payload                                       |
|-----------------------------|----------------------|-----------------------------------------------|
| Success                     | `CMD_ADMIN_RESP`     | `"OK"`                                        |
| Fewer than 7 fields         | `CMD_ERROR`          | `"Add requires 7 fields..."`                  |
| Empty code/section          | `CMD_ERROR`          | `"Course code and section cannot be empty"`   |
| DB write failure            | `CMD_SERVER_ERROR`   | `"Failed to insert course: <details>"`        |
| Insufficient permissions    | `CMD_PERMISSION_ERR` | `"Insufficient permissions"` (dispatcher).    |

---

### UPDATE (admin)

**Request** `CMD_UPDATE_REQ` (C2S, role=ADMIN)

Same fields as ADD. `(code, section)` locates the row; the remaining five fields are the new values.

**Response**

| Case                        | cmd_type             | payload                                       |
|-----------------------------|----------------------|-----------------------------------------------|
| Success                     | `CMD_ADMIN_RESP`     | `"OK"`                                        |
| Fewer than 7 fields         | `CMD_ERROR`          | `"Update requires 7 fields"`                  |
| Empty code/section          | `CMD_ERROR`          | `"Course code and section cannot be empty"`   |
| DB update failure           | `CMD_SERVER_ERROR`   | `"Failed to update course: <details>"`        |
| Insufficient permissions    | `CMD_PERMISSION_ERR` | (dispatcher)                                  |

Under the current schema the "row not found" case is not surfaced explicitly; it depends on
whether `CourseRepository::update` checks affected rows.

---

### DELETE (admin)

**Request** `CMD_DELETE_REQ` (C2S, role=ADMIN)

| Index     | Field    | Type  | Description    |
|-----------|----------|-------|----------------|
| `json[0]` | code     | bytes | Course code.   |
| `json[1]` | section  | bytes | Section/class. |

**Response**

| Case                        | cmd_type             | payload                                       |
|-----------------------------|----------------------|-----------------------------------------------|
| Success                     | `CMD_ADMIN_RESP`     | `"OK"`                                        |
| Fewer than 2 fields         | `CMD_ERROR`          | `"Delete requires code and section"`          |
| Empty field                 | `CMD_ERROR`          | `"Course code and section cannot be empty"`   |
| DB delete failure           | `CMD_SERVER_ERROR`   | `"Failed to delete course: <details>"`        |
| Insufficient permissions    | `CMD_PERMISSION_ERR` | (dispatcher)                                  |

---

## Invariants

1. Direction-exclusive: C2S never originates from the server, S2C never from the client. Validate with `is_c2s` / `is_s2c`.
2. Every response carries a `cmd_type`.
3. The dispatcher never throws; unknown exceptions become `CMD_SERVER_ERROR`.
4. Authorization lives only in `Dispatcher::dispatch`; handlers assume the check passed.
5. `req_id` is unique per frame (atomic counter) for request/response correlation.
6. Any receive failure tears down the connection; network and protocol errors are not distinguished.

---

## Sample sequences

### Student queries a course

```
Client                           Server
  | -- CMD_QUERY_BY_CODE_REQ -->  |
  |   json=["CS101"]              |  (Session.role=STUDENT, OK)
  |                               |  CourseRepo.search_by_course_code
  | <-- CMD_QUERY_RESP ---------- |
  |   payload="2<RS>CS101<US>..." |
```

### Admin login then add/delete

```
Client                           Server
  | -- CMD_LOGIN_REQ ----------->  |
  |   json=["alice","secret123"]   |
  |                                |  AdminRepo.verify_login
  | <-- CMD_LOGIN_RESP ----------  |  Session.role=ADMIN
  |
  | -- CMD_ADD_REQ ------------->  |
  |   json=[7 fields]              |
  | <-- CMD_ADMIN_RESP ----------  |  payload="OK"
  |
  | -- CMD_LOGOUT_REQ ---------->  |
  | <-- CMD_OK -----------------   |  Session downgraded.
```

### Student attempts a privileged action

```
Client                           Server
  | -- CMD_DELETE_REQ ---------->  |  (Session.role=STUDENT)
  |   json=["CS101","A"]           |  Dispatcher rejects.
  | <-- CMD_PERMISSION_ERR ------  |  payload="Insufficient permissions"
                                       (DB untouched)
```

---

## Implementation index

| Topic                       | File                                        |
|-----------------------------|---------------------------------------------|
| Header / body structures    | `Driver/include/protocol.h`                 |
| Send / receive impl         | `Driver/src/protocol.cpp`                   |
| Command enum                | `Driver/include/cmd_type.h`                 |
| Dispatcher                  | `Driver/src/CmdHandler.cpp`                 |
| Server handlers             | `programs/Server/src/cmdreg.cpp`            |
| Server registration entry   | `register_all_server()` in `cmdreg.cpp`     |
| Unit tests                  | `Tests/test_cmdreg.cpp`, `Tests/test_protocol.cpp` |
