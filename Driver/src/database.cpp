#include "database.h"

#include <sqlite3.h>

#include "protocol.h"

namespace dcn_protocol {

CourseStore::CourseStore(const std::string& db_path)
    : db_(nullptr), ready_(false) {
    ready_ = open_database(db_path) && initialize_schema();
}

CourseStore::~CourseStore() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool CourseStore::open_database(const std::string& db_path) {
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }
    return true;
}

bool CourseStore::initialize_schema() {
    if (db_ == nullptr) {
        return false;
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS courses ("
        "code TEXT PRIMARY KEY,"
        "title TEXT NOT NULL,"
        "section TEXT NOT NULL,"
        "instructor TEXT NOT NULL,"
        "time TEXT NOT NULL,"
        "classroom TEXT NOT NULL"
        ");";

    char* error = nullptr;
    const int return_code = sqlite3_exec(db_, sql, nullptr, nullptr, &error);
    if (return_code != SQLITE_OK) {
        sqlite3_free(error);
        return false;
    }
    return true;
}

bool CourseStore::is_ready() const {
    return ready_;
}

std::vector<std::string> CourseStore::process_command(const std::string& line) {
    std::vector<std::string> resp;
    const ParsedRequest req = parse_request(line);
    const std::vector<std::string>& parts = req.parts;
    if (parts.empty()) {
        resp.push_back(make_response(ResponseType::Error, {"Empty command"}));
        return resp;
    }
    std::lock_guard<std::mutex> lock(courses_mutex_);

    if (!ready_ || db_ == nullptr) {
        resp.push_back(make_response(ResponseType::Error, {"Database not ready"}));
        return resp;
    }

    if (req.type == CommandType::Help) {
        resp.push_back(make_response(ResponseType::Ok, {command_help()}));
        return resp;
    }

    if (req.type == CommandType::Insert) {
        if (parts.size() != 7) {
            resp.push_back(make_response(ResponseType::Error, {usage_for(CommandType::Insert)}));
            return resp;
        }

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO courses(code, title, section, instructor, time, classroom) VALUES(?, ?, ?, ?, ?, ?);";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            resp.push_back(make_response(ResponseType::Error, {"Database prepare failed"}));
            return resp;
        }

        sqlite3_bind_text(stmt, 1, parts[1].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, parts[2].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, parts[3].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, parts[4].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, parts[5].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, parts[6].c_str(), -1, SQLITE_TRANSIENT);

        const int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc == SQLITE_CONSTRAINT) {
            resp.push_back(make_response(ResponseType::Error, {"Course already exists"}));
            return resp;
        }
        if (rc != SQLITE_DONE) {
            resp.push_back(make_response(ResponseType::Error, {"Insert failed"}));
            return resp;
        }

        resp.push_back(make_response(ResponseType::Ok, {"Inserted"}));
        return resp;
    }

    if (req.type == CommandType::Update) {
        if (parts.size() != 7) {
            resp.push_back(make_response(ResponseType::Error, {usage_for(CommandType::Update)}));
            return resp;
        }

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "UPDATE courses SET title=?, section=?, instructor=?, time=?, classroom=? WHERE code=?;";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            resp.push_back(make_response(ResponseType::Error, {"Database prepare failed"}));
            return resp;
        }

        sqlite3_bind_text(stmt, 1, parts[2].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, parts[3].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, parts[4].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, parts[5].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, parts[6].c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, parts[1].c_str(), -1, SQLITE_TRANSIENT);

        const int rc = sqlite3_step(stmt);
        const int changed = sqlite3_changes(db_);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            resp.push_back(make_response(ResponseType::Error, {"Update failed"}));
            return resp;
        }
        if (changed == 0) {
            resp.push_back(make_response(ResponseType::Error, {"Course not found"}));
            return resp;
        }

        resp.push_back(make_response(ResponseType::Ok, {"Updated"}));
        return resp;
    }

    if (req.type == CommandType::Delete) {
        if (parts.size() != 2) {
            resp.push_back(make_response(ResponseType::Error, {usage_for(CommandType::Delete)}));
            return resp;
        }

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "DELETE FROM courses WHERE code=?;";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            resp.push_back(make_response(ResponseType::Error, {"Database prepare failed"}));
            return resp;
        }

        sqlite3_bind_text(stmt, 1, parts[1].c_str(), -1, SQLITE_TRANSIENT);
        const int rc = sqlite3_step(stmt);
        const int changed = sqlite3_changes(db_);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            resp.push_back(make_response(ResponseType::Error, {"Delete failed"}));
            return resp;
        }
        if (changed == 0) {
            resp.push_back(make_response(ResponseType::Error, {"Course not found"}));
            return resp;
        }

        resp.push_back(make_response(ResponseType::Ok, {"Deleted"}));
        return resp;
    }

    if (req.type == CommandType::Query) {
        if (parts.size() == 2 && to_upper(parts[1]) == "ALL") {
            sqlite3_stmt* count_stmt = nullptr;
            if (sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM courses;", -1, &count_stmt, nullptr) != SQLITE_OK) {
                resp.push_back(make_response(ResponseType::Error, {"Query failed"}));
                return resp;
            }

            int total = 0;
            if (sqlite3_step(count_stmt) == SQLITE_ROW) {
                total = sqlite3_column_int(count_stmt, 0);
            }
            sqlite3_finalize(count_stmt);

            resp.push_back(make_response(ResponseType::Data, {std::to_string(total)}));

            sqlite3_stmt* stmt = nullptr;
            const char* sql = "SELECT code, title, section, instructor, time, classroom FROM courses ORDER BY code;";
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                resp.push_back(make_response(ResponseType::Error, {"Query failed"}));
                return resp;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Course c;
                c.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                c.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                c.section = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                c.instructor = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                c.time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                c.classroom = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
                resp.push_back(make_response(ResponseType::Row, {
                    c.code, c.title, c.section, c.instructor, c.time, c.classroom
                }));
            }
            sqlite3_finalize(stmt);

            resp.push_back(make_response(ResponseType::End));
            return resp;
        }

        if (parts.size() == 3 && to_upper(parts[1]) == "CODE") {
            sqlite3_stmt* stmt = nullptr;
            const char* sql = "SELECT code, title, section, instructor, time, classroom FROM courses WHERE code=?;";
            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                resp.push_back(make_response(ResponseType::Error, {"Query failed"}));
                return resp;
            }
            sqlite3_bind_text(stmt, 1, parts[2].c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_ROW) {
                sqlite3_finalize(stmt);
                resp.push_back(make_response(ResponseType::Error, {"Course not found"}));
                return resp;
            }

            Course c;
            c.code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            c.title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            c.section = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            c.instructor = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            c.time = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            c.classroom = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            sqlite3_finalize(stmt);

            resp.push_back(make_response(ResponseType::Data, {"1"}));
            resp.push_back(make_response(ResponseType::Row, {
                c.code, c.title, c.section, c.instructor, c.time, c.classroom
            }));
            resp.push_back(make_response(ResponseType::End));
            return resp;
        }

        resp.push_back(make_response(ResponseType::Error, {usage_for(CommandType::Query)}));
        return resp;
    }

    resp.push_back(make_response(ResponseType::Error, {"Unknown command"}));
    return resp;
}

}  // namespace dcn_protocol
