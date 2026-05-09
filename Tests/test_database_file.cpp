#include "database.h"
#include "test.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;


// ─────────────────────────────────────────────
// 辅助：用完自动删除数据库文件
// ─────────────────────────────────────────────
struct TempDb {
    std::string path;
    explicit TempDb(const std::string& name) : path(name) {
        fs::create_directories(fs::path(path).parent_path());  // 确保目录存在
        fs::remove(path);   // 确保测试前是干净的
    }
    ~TempDb() {
        // fs::remove(path);   // 注释掉就不删了
    }
};

// ─────────────────────────────────────────────
// 文件数据库测试
// ─────────────────────────────────────────────

// 写入数据后关闭，再重新打开，数据应仍然存在
TEST(test_course_data_persists_across_reopen) {
    TempDb tmp("data/test_course.db");

    // 第一次打开：写入数据
    {
        dcn_database::CourseRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());

        dcn_database::Course c;
        c.code       = "CS101";
        c.title      = "Intro to CS";
        c.section    = "A";
        c.instructor = "Dr. Smith";
        c.day        = "Monday";
        c.duration   = "09:00-10:30";
        c.classroom  = "Room 1";
        REQUIRE(repo.insert_or_replace(c));
    }   // repo 析构 → 连接关闭

    // 确认文件真的落到磁盘
    REQUIRE(fs::exists(tmp.path));
    REQUIRE(fs::file_size(tmp.path) > 0);

    // 第二次打开：读取数据
    {
        dcn_database::CourseRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());   // CREATE TABLE IF NOT EXISTS，不会清空

        auto all = repo.view_all_courses();
        REQUIRE(all.size() == 1);
        REQUIRE(all[0].code      == "CS101");
        REQUIRE(all[0].title     == "Intro to CS");
        REQUIRE(all[0].day       == "Monday");
        REQUIRE(all[0].duration  == "09:00-10:30");
    }
}

// 多次写入累积后关闭再重开，总数正确
TEST(test_course_accumulates_across_sessions) {
    TempDb tmp("data/test_course_multi.db");

    // 第一次会话：写 2 条
    {
        dcn_database::CourseRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());

        for (const char* sec : {"A", "B"}) {
            dcn_database::Course c;
            c.code = "CS202"; c.title = "Algorithms"; c.section = sec;
            c.instructor = "Prof. Lee"; c.day = "Wed"; c.duration = "14:00"; c.classroom = "R2";
            REQUIRE(repo.insert_or_replace(c));
        }
    }

    // 第二次会话：再写 1 条
    {
        dcn_database::CourseRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());

        dcn_database::Course c;
        c.code = "CS202"; c.title = "Algorithms"; c.section = "C";
        c.instructor = "Prof. Lee"; c.day = "Wed"; c.duration = "14:00"; c.classroom = "R2";
        REQUIRE(repo.insert_or_replace(c));
    }

    // 第三次会话：确认总共 3 条
    {
        dcn_database::CourseRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());

        auto results = repo.search_by_course_code("CS202");
        REQUIRE(results.size() == 3);
    }
}

// 管理员登录持久化
TEST(test_admin_login_persists_across_reopen) {
    TempDb tmp("data/test_admin.db");

    // 第一次会话：写入管理员
    {
        dcn_database::AdministratorRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());

        dcn_database::Administrator admin;
        admin.username = "admin";
        admin.password = "pass2026";
        REQUIRE(repo.insert_or_replace(admin));
    }

    REQUIRE(fs::exists(tmp.path));

    // 第二次会话：验证登录
    {
        dcn_database::AdministratorRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());

        REQUIRE(repo.verify_login("admin", "pass2026"));
        REQUIRE(!repo.verify_login("admin", "wrong"));
    }
}

// 密码更新后重开，旧密码失效
TEST(test_admin_password_update_persists) {
    TempDb tmp("data/test_admin_update.db");

    {
        dcn_database::AdministratorRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());
        dcn_database::Administrator admin;
        admin.username = "bob"; admin.password = "old";
        REQUIRE(repo.insert_or_replace(admin));
    }

    {
        dcn_database::AdministratorRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());
        dcn_database::Administrator admin;
        admin.username = "bob"; admin.password = "new";
        REQUIRE(repo.insert_or_replace(admin));   // 覆盖
    }

    {
        dcn_database::AdministratorRepository repo(tmp.path);
        REQUIRE(repo.initialize_schema());
        REQUIRE(!repo.verify_login("bob", "old"));
        REQUIRE(repo.verify_login("bob", "new"));
    }
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    std::cout << "=== File-based Database Tests ===\n\n";

    std::cout << "--- CourseRepository ---\n";
    RUN(test_course_data_persists_across_reopen);
    RUN(test_course_accumulates_across_sessions);

    std::cout << "\n--- AdministratorRepository ---\n";
    RUN(test_admin_login_persists_across_reopen);
    RUN(test_admin_password_update_persists);

    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}
