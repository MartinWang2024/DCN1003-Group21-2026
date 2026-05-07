#pragma once

#include <mutex>
#include <string>
#include <vector>

struct sqlite3;

namespace dcn_protocol {

class CourseStore {
public:
    explicit CourseStore(const std::string& db_path = "timetable.db");
    ~CourseStore();

    bool is_ready() const;
    std::vector<std::string> process_command(const std::string& line);

private:
    bool open_database(const std::string& db_path);
    bool initialize_schema();

    sqlite3* db_;
    bool ready_;
    std::mutex courses_mutex_;
};

}  // namespace dcn_protocol
