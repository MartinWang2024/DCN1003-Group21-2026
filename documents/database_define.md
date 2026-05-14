Ran terminal command: sqlite3 data/DCN.db ".schema"

**`administrators`**
| 列 | 类型 | 约束 |
|---|---|---|
| `username` | VARCHAR(64) | PRIMARY KEY |
| `password` | VARCHAR(255) | NOT NULL |

---

**`courses`**
| 列 | 类型 | 约束 |
|---|---|---|
| `code` | VARCHAR(32) | PK (part) |
| `title` | VARCHAR(128) | NOT NULL |
| `section` | VARCHAR(32) | PK (part) |
| `instructor` | VARCHAR(64) | NOT NULL |

主键：`(code, section)`

---

**`schedules`**
| 列 | 类型 | 约束 |
|---|---|---|
| `course_code` | VARCHAR(32) | PK (part) |
| `section` | VARCHAR(32) | PK (part) |
| `day` | DATE | PK (part) |
| `duration` | VARCHAR(32) | PK (part) |
| `semester` | VARCHAR(16) | PK (part), DEFAULT `''` |
| `classroom` | VARCHAR(64) | NOT NULL, DEFAULT `''` |

主键：`(course_code, section, day, duration, semester)`  
外键：`(course_code, section)` → `courses(code, section)` ON DELETE/UPDATE CASCADE