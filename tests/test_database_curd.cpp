#include "database.h"
#include "test.h"

#include <iostream>
#include <string>
#include <vector>

// ─── CourseRepository CRUD ───────────────────────────────────────────────────

TEST(test_course_initialize_schema) {
    dcn_database::CourseRepository repo;
    REQUIRE(repo.open("data/Course.db"));
    REQUIRE(repo.initialize_schema());
    REQUIRE(repo.last_error().empty());
    std::cout << "  Course schema initialized.\n";
}

TEST(test_course_insert) {
    dcn_database::CourseRepository repo;
    REQUIRE(repo.open("data/Course.db"));
    REQUIRE(repo.initialize_schema());

    dcn_database::Course c;
    c.code       = "DCN1003";
    c.title      = "Data Communication Networks";
    c.section    = "G21";
    c.instructor = "Dr. Smith";
    c.day        = "Monday";
    c.duration   = "2h";
    c.classroom  = "Lab A";

    REQUIRE(repo.insert_or_replace(c));
    REQUIRE(repo.last_error().empty());
    std::cout << "  Inserted course DCN1003-G21.\n";
}

TEST(test_course_query_by_code) {
    dcn_database::CourseRepository repo;
    REQUIRE(repo.open("data/Course.db"));

    auto results = repo.search_by_course_code("DCN1003");
    REQUIRE(repo.last_error().empty());
    REQUIRE(!results.empty());
    REQUIRE(results.front().code == "DCN1003");
    REQUIRE(results.front().section == "G21");
    std::cout << "  Query by code: found " << results.size() << " record(s).\n";
}

TEST(test_course_query_by_instructor) {
    dcn_database::CourseRepository repo;
    REQUIRE(repo.open("data/Course.db"));

    auto results = repo.search_by_instructor("Smith");
    REQUIRE(repo.last_error().empty());
    REQUIRE(!results.empty());
    std::cout << "  Query by instructor 'Smith': found " << results.size() << " record(s).\n";
}

TEST(test_course_update) {
    dcn_database::CourseRepository repo;
    REQUIRE(repo.open("data/Course.db"));

    dcn_database::Course c;
    c.code       = "DCN1003";
    c.section    = "G21";
    c.title      = "Data Communication Networks (Updated)";
    c.instructor = "Dr. Johnson";
    c.day        = "Tuesday";
    c.duration   = "3h";
    c.classroom  = "Lab B";

    REQUIRE(repo.update(c));
    REQUIRE(repo.last_error().empty());

    auto results = repo.search_by_course_code("DCN1003");
    REQUIRE(!results.empty());
    REQUIRE(results.front().instructor == "Dr. Johnson");
    REQUIRE(results.front().classroom == "Lab B");
    std::cout << "  Updated course DCN1003-G21, instructor now: " << results.front().instructor << "\n";
}

TEST(test_course_view_all) {
    dcn_database::CourseRepository repo;
    REQUIRE(repo.open("data/Course.db"));

    auto all = repo.view_all_courses();
    REQUIRE(repo.last_error().empty());
    std::cout << "  view_all_courses: " << all.size() << " record(s).\n";
    for (const auto& c : all) {
        std::cout << "    [" << c.code << " " << c.section << "] " << c.title
                  << " / " << c.instructor << "\n";
    }
}

TEST(test_course_delete) {
    dcn_database::CourseRepository repo;
    REQUIRE(repo.open("data/Course.db"));

    REQUIRE(repo.delete_course("DCN1003", "G21"));
    REQUIRE(repo.last_error().empty());

    auto results = repo.search_by_course_code("DCN1003");
    REQUIRE(results.empty());
    std::cout << "  Deleted course DCN1003-G21, remaining: " << results.size() << "\n";
}

// ─── AdministratorRepository CRUD ────────────────────────────────────────────

TEST(test_admin_initialize_schema) {
    dcn_database::AdministratorRepository repo;
    REQUIRE(repo.open("data/Admin.db"));
    REQUIRE(repo.initialize_schema());
    REQUIRE(repo.last_error().empty());
    std::cout << "  Admin schema initialized.\n";
}

TEST(test_admin_insert_and_verify) {
    dcn_database::AdministratorRepository repo;
    REQUIRE(repo.open("data/Admin.db"));
    REQUIRE(repo.initialize_schema());

    dcn_database::Administrator admin;
    admin.username = "testadmin";
    admin.password = "password123";

    REQUIRE(repo.insert_or_replace(admin));
    REQUIRE(repo.last_error().empty());
    std::cout << "  Inserted admin 'testadmin'.\n";

    // 正确密码应验证通过
    REQUIRE(repo.verify_login("testadmin", "password123"));
    std::cout << "  verify_login with correct password: PASS\n";

    // 错误密码应验证失败
    bool wrong = repo.verify_login("testadmin", "wrongpassword");
    REQUIRE(!wrong);
    std::cout << "  verify_login with wrong password: correctly rejected\n";
}

TEST(test_admin_insert_or_replace_update) {
    dcn_database::AdministratorRepository repo;
    REQUIRE(repo.open("data/Admin.db"));

    // 同一 username 再次插入（覆盖密码）
    dcn_database::Administrator admin;
    admin.username = "testadmin";
    admin.password = "newpassword456";

    REQUIRE(repo.insert_or_replace(admin));
    REQUIRE(repo.last_error().empty());

    // 旧密码失效
    REQUIRE(!repo.verify_login("testadmin", "password123"));
    // 新密码有效
    REQUIRE(repo.verify_login("testadmin", "newpassword456"));
    std::cout << "  Admin password updated via insert_or_replace: PASS\n";
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main() {
    std::cout << "=== Database CRUD Test ===\n";

    std::cout << "\n-- CourseRepository --\n";
    RUN(test_course_initialize_schema);
    RUN(test_course_insert);
    RUN(test_course_query_by_code);
    RUN(test_course_query_by_instructor);
    RUN(test_course_update);
    RUN(test_course_view_all);
    RUN(test_course_delete);

    std::cout << "\n-- AdministratorRepository --\n";
    RUN(test_admin_initialize_schema);
    RUN(test_admin_insert_and_verify);
    RUN(test_admin_insert_or_replace_update);

    std::cout << "\n--- Result: " << s_passed << " passed, " << s_failed << " failed ---\n";
    return s_failed == 0 ? 0 : 1;
}
