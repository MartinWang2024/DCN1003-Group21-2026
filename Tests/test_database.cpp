#include "database.h"
#include "test.h"
#include <cassert>
#include <iostream>
#include <string>

// ─────────────────────────────────────────────
// CourseRepository 测试
// ─────────────────────────────────────────────
TEST(test_course_open_and_schema) {
    dcn_database::CourseRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());
}

TEST(test_course_insert_and_view_all) {
    dcn_database::CourseRepository repo(":memory:");
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

    auto all = repo.view_all_courses();
    REQUIRE(all.size() == 1);
    REQUIRE(all[0].code       == "CS101");
    REQUIRE(all[0].title      == "Intro to CS");
    REQUIRE(all[0].section    == "A");
    REQUIRE(all[0].instructor == "Dr. Smith");
    REQUIRE(all[0].day        == "Monday");
    REQUIRE(all[0].duration   == "09:00-10:30");
    REQUIRE(all[0].classroom  == "Room 1");
}

TEST(test_course_insert_multiple_sections) {
    dcn_database::CourseRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    for (const char* sec : {"A", "B", "C"}) {
        dcn_database::Course c;
        c.code       = "CS101";
        c.title      = "Intro to CS";
        c.section    = sec;
        c.instructor = "Dr. Smith";
        c.day        = "Tuesday";
        c.duration   = "10:00-11:30";
        c.classroom  = "Room 2";
        REQUIRE(repo.insert_or_replace(c));
    }

    auto results = repo.search_by_course_code("CS101");
    REQUIRE(results.size() == 3);
}

TEST(test_course_search_by_course_code_not_found) {
    dcn_database::CourseRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    auto results = repo.search_by_course_code("UNKNOWN");
    REQUIRE(results.empty());
}

TEST(test_course_search_by_instructor) {
    dcn_database::CourseRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    dcn_database::Course c1;
    c1.code = "CS101"; c1.title = "Intro to CS"; c1.section = "A";
    c1.instructor = "Dr. Smith"; c1.day = "Mon"; c1.duration = "09:00"; c1.classroom = "R1";

    dcn_database::Course c2;
    c2.code = "CS202"; c2.title = "Algorithms"; c2.section = "A";
    c2.instructor = "Prof. Jones"; c2.day = "Wed"; c2.duration = "14:00"; c2.classroom = "R2";

    REQUIRE(repo.insert_or_replace(c1));
    REQUIRE(repo.insert_or_replace(c2));

    // 精确匹配
    auto r1 = repo.search_by_instructor("Smith");
    REQUIRE(r1.size() == 1);
    REQUIRE(r1[0].code == "CS101");

    // 部分匹配（LIKE %Jones%）
    auto r2 = repo.search_by_instructor("Jones");
    REQUIRE(r2.size() == 1);
    REQUIRE(r2[0].code == "CS202");
}

TEST(test_course_insert_or_replace_updates_existing) {
    dcn_database::CourseRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    dcn_database::Course c;
    c.code = "CS101"; c.title = "Old Title"; c.section = "A";
    c.instructor = "Dr. A"; c.day = "Mon"; c.duration = "09:00"; c.classroom = "R1";
    REQUIRE(repo.insert_or_replace(c));

    c.title      = "New Title";
    c.instructor = "Dr. B";
    REQUIRE(repo.insert_or_replace(c));   // 同 (code, section) → 应覆盖

    auto all = repo.view_all_courses();
    REQUIRE(all.size() == 1);
    REQUIRE(all[0].title      == "New Title");
    REQUIRE(all[0].instructor == "Dr. B");
}

// ─────────────────────────────────────────────
// AdministratorRepository 测试
// ─────────────────────────────────────────────
TEST(test_admin_open_and_schema) {
    dcn_database::AdministratorRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());
}

TEST(test_admin_insert_and_verify_login_success) {
    dcn_database::AdministratorRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    dcn_database::Administrator admin;
    admin.username = "alice";
    admin.password = "secret123";
    REQUIRE(repo.insert_or_replace(admin));

    REQUIRE(repo.verify_login("alice", "secret123"));
}

TEST(test_admin_verify_login_wrong_password) {
    dcn_database::AdministratorRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    dcn_database::Administrator admin;
    admin.username = "alice";
    admin.password = "secret123";
    REQUIRE(repo.insert_or_replace(admin));

    REQUIRE(!repo.verify_login("alice", "wrong"));
}

TEST(test_admin_verify_login_unknown_user) {
    dcn_database::AdministratorRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    REQUIRE(!repo.verify_login("nobody", "anything"));
}

TEST(test_admin_insert_or_replace_updates_password) {
    dcn_database::AdministratorRepository repo(":memory:");
    REQUIRE(repo.initialize_schema());

    dcn_database::Administrator admin;
    admin.username = "bob";
    admin.password = "old_pass";
    REQUIRE(repo.insert_or_replace(admin));

    admin.password = "new_pass";
    REQUIRE(repo.insert_or_replace(admin));  // 同 username → 应覆盖

    REQUIRE(!repo.verify_login("bob", "old_pass"));
    REQUIRE(repo.verify_login("bob", "new_pass"));
}

// ─────────────────────────────────────────────
// main
// ─────────────────────────────────────────────
int main() {
    std::cout << "=== CourseRepository ===\n";
    RUN(test_course_open_and_schema);
    RUN(test_course_insert_and_view_all);
    RUN(test_course_insert_multiple_sections);
    RUN(test_course_search_by_course_code_not_found);
    RUN(test_course_search_by_instructor);
    RUN(test_course_insert_or_replace_updates_existing);

    std::cout << "\n=== AdministratorRepository ===\n";
    RUN(test_admin_open_and_schema);
    RUN(test_admin_insert_and_verify_login_success);
    RUN(test_admin_verify_login_wrong_password);
    RUN(test_admin_verify_login_unknown_user);
    RUN(test_admin_insert_or_replace_updates_password);

    std::cout << "\n--- Result: "
              << s_passed << " passed, "
              << s_failed << " failed ---\n";

    return s_failed == 0 ? 0 : 1;
}
