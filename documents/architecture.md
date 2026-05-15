# Architecture

Server-side dispatch pipeline and database schema.

## Dispatch pipeline

```
+--------------+   MsgBody    +-----------------+   business call  +--------------+
| Package_recv | -----------> |   Dispatcher    | ---------------> |   Handlers   |
|              |              | (route by cmd)  |                  | (LOGIN/QUERY)|
+--------------+              +-----------------+                  +--------------+
                                       |                                   |
                                       |  MsgBody (response)               |
                                       <-----------------------------------+
                                       |
                              +--------------+
                              | Package_send |
                              +--------------+
```

## Layers

```
L4  Connection loop (per-thread)
        while (alive) { recv -> dispatch -> send }

L3  Dispatcher
        Route cmd_type to handler.
        Authorization (ADMIN / STUDENT).
        Catch exceptions, return CMD_SERVER_ERROR.

L2  Handlers (LoginHandler / QueryHandler / ...)
        Parse payload, call repository, build response.

L1  Repositories (CourseRepository, AdministratorRepository)
```

Authorization runs only inside `Dispatcher::dispatch()`. Handlers assume the check passed.

## Schema

Source of truth: `sqlite3 data/DCN.db ".schema"`.

### `administrators`

| Column     | Type         | Constraints |
|------------|--------------|-------------|
| `username` | VARCHAR(64)  | PRIMARY KEY |
| `password` | VARCHAR(255) | NOT NULL    |

Passwords stored as PBKDF2-HMAC-SHA256 (`driver/src/password_hash.cpp`):
`pbkdf2$<iter>$<saltHex>$<hashHex>`, iter=120000, salt=16B, key=32B.

### `courses`

| Column       | Type         | Constraints      |
|--------------|--------------|------------------|
| `code`       | VARCHAR(32)  | PK (part)        |
| `section`    | VARCHAR(32)  | PK (part)        |
| `title`      | VARCHAR(128) | NOT NULL         |
| `instructor` | VARCHAR(64)  | NOT NULL         |

Primary key: `(code, section)`.

### `schedules`

| Column        | Type        | Constraints                |
|---------------|-------------|----------------------------|
| `course_code` | VARCHAR(32) | PK (part)                  |
| `section`     | VARCHAR(32) | PK (part)                  |
| `day`         | DATE        | PK (part)                  |
| `duration`    | VARCHAR(32) | PK (part)                  |
| `semester`    | VARCHAR(16) | PK (part), DEFAULT `''`    |
| `classroom`   | VARCHAR(64) | NOT NULL, DEFAULT `''`     |

Primary key: `(course_code, section, day, duration, semester)`.
Foreign key: `(course_code, section) -> courses(code, section)` ON DELETE/UPDATE CASCADE.

All write paths run inside an explicit transaction (BEGIN/COMMIT/ROLLBACK). Connection
opens with WAL mode, foreign keys ON, FULLMUTEX threading.
