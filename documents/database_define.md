Ran terminal command: sqlite3 data/DCN.db ".schema"

**`administrators`**
| Column | Type | Constraints |
|---|---|---|
| `username` | VARCHAR(64) | PRIMARY KEY |
| `password` | VARCHAR(255) | NOT NULL |

---

**`courses`**
| Column | Type | Constraints |
|---|---|---|
| `code` | VARCHAR(32) | PK (part) |
| `title` | VARCHAR(128) | NOT NULL |
| `section` | VARCHAR(32) | PK (part) |
| `instructor` | VARCHAR(64) | NOT NULL |

Primary key: `(code, section)`

---

**`schedules`**
| Column | Type | Constraints |
|---|---|---|
| `course_code` | VARCHAR(32) | PK (part) |
| `section` | VARCHAR(32) | PK (part) |
| `day` | DATE | PK (part) |
| `duration` | VARCHAR(32) | PK (part) |
| `semester` | VARCHAR(16) | PK (part), DEFAULT `''` |
| `classroom` | VARCHAR(64) | NOT NULL, DEFAULT `''` |

Primary key: `(course_code, section, day, duration, semester)`
Foreign key: `(course_code, section)` -> `courses(code, section)` ON DELETE/UPDATE CASCADE
