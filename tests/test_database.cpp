#include "test.h"

#include <iostream>
#include <stdexcept>
#include <string>

#include "database.h"

using namespace dcn_database;

// ─────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────

static CourseRepository make_course_repo() {
	CourseRepository repo(":memory:");
	if (!repo.initialize_schema())
		throw std::runtime_error("initialize_schema failed: " + repo.last_error());
	return repo;
}

static AdministratorRepository make_admin_repo() {
	AdministratorRepository repo(":memory:");
	if (!repo.initialize_schema())
		throw std::runtime_error("initialize_schema failed: " + repo.last_error());
	return repo;
}

static Course make_course(
	const std::string& code       = "CS101",
	const std::string& title      = "Intro to CS",
	const std::string& section    = "A",
	const std::string& instructor = "Dr. Smith",
	const std::string& day        = "Monday",
	const std::string& duration   = "09:00-10:30",
	const std::string& semester   = "2026S1",
	const std::string& classroom  = "LT1") {
	return {code, title, section, instructor, day, duration, semester, classroom};
}

// ─────────────────────────────────────────────────────────────
// Database
// ─────────────────────────────────────────────────────────────

TEST(db_open_memory) {
	Database db;
	REQUIRE(db.open(":memory:"));
	REQUIRE(db.raw_handle() != nullptr);
}

TEST(db_close_nulls_handle) {
	Database db;
	REQUIRE(db.open(":memory:"));
	db.close();
	REQUIRE(db.raw_handle() == nullptr);
}

TEST(db_double_close_is_safe) {
	Database db;
	REQUIRE(db.open(":memory:"));
	db.close();
	db.close();  // must not crash or assert
}

TEST(db_open_invalid_path_returns_false) {
	Database db;
	// Parent directory does not exist; SQLite cannot create the file
	REQUIRE(!db.open("./no_such_dir_abc999xyz/nested/db.sqlite"));
	REQUIRE(!db.last_error().empty());
}

TEST(db_reopen_after_close) {
	Database db;
	REQUIRE(db.open(":memory:"));
	db.close();
	REQUIRE(db.open(":memory:"));
}

// ─────────────────────────────────────────────────────────────
// CourseRepository — schema & basic insert/query
// ─────────────────────────────────────────────────────────────

TEST(course_initialize_schema) {
	CourseRepository repo(":memory:");
	REQUIRE(repo.initialize_schema());
}

TEST(course_initialize_schema_idempotent) {
	CourseRepository repo(":memory:");
	REQUIRE(repo.initialize_schema());
	REQUIRE(repo.initialize_schema());  // IF NOT EXISTS — must succeed twice
}

TEST(course_insert_and_search_by_code) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course()));

	auto results = repo.search_by_course_code("CS101");
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].code       == "CS101");
	REQUIRE(results[0].title      == "Intro to CS");
	REQUIRE(results[0].section    == "A");
	REQUIRE(results[0].instructor == "Dr. Smith");
	REQUIRE(results[0].day        == "Monday");
	REQUIRE(results[0].duration   == "09:00-10:30");
	REQUIRE(results[0].semester   == "2026S1");
	REQUIRE(results[0].classroom  == "LT1");
}

TEST(course_insert_conflict_updates_title_and_instructor) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course()));

	// Same (code, section) key — title and instructor should be overwritten
	Course updated = make_course();
	updated.title      = "Advanced CS";
	updated.instructor = "Dr. Jones";
	REQUIRE(repo.insert_or_replace(updated));

	auto results = repo.search_by_course_code("CS101");
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].title      == "Advanced CS");
	REQUIRE(results[0].instructor == "Dr. Jones");
}

TEST(course_insert_conflict_updates_classroom) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course()));

	// Same composite schedule key — classroom should be overwritten
	Course updated = make_course();
	updated.classroom = "LT99";
	REQUIRE(repo.insert_or_replace(updated));

	auto results = repo.search_by_course_code("CS101");
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].classroom == "LT99");
}

TEST(course_search_by_instructor_exact) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t1", "A", "Dr. Smith")));
	REQUIRE(repo.insert_or_replace(make_course("CS102", "t2", "B", "Dr. Lee", "Tue", "1h", "S1", "R2")));

	auto results = repo.search_by_instructor("Dr. Smith");
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].code == "CS101");
}

TEST(course_search_by_instructor_partial) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t1", "A", "Dr. Smith")));
	REQUIRE(repo.insert_or_replace(make_course("CS102", "t2", "B", "Dr. Lee", "Tue", "1h", "S1", "R2")));

	auto results = repo.search_by_instructor("Dr.");
	REQUIRE(results.size() == 2);
}

TEST(course_search_by_classroom_partial) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Mon", "1h", "S1", "LT1")));
	REQUIRE(repo.insert_or_replace(make_course("CS102", "t", "B", "i", "Tue", "1h", "S1", "LT2")));

	auto r1 = repo.search_by_classroom("LT1");
	REQUIRE(r1.size() == 1);
	REQUIRE(r1[0].classroom == "LT1");

	auto rall = repo.search_by_classroom("LT");
	REQUIRE(rall.size() == 2);
}

TEST(course_search_by_day) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Monday",  "1h", "S1", "R1")));
	REQUIRE(repo.insert_or_replace(make_course("CS102", "t", "B", "i", "Tuesday", "1h", "S1", "R2")));

	auto results = repo.search_by_day("Monday");
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].code == "CS101");

	REQUIRE(repo.search_by_day("Sunday").empty());
}

TEST(course_view_by_semester) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Mon", "1h", "2026S1", "R1")));
	REQUIRE(repo.insert_or_replace(make_course("CS102", "t", "B", "i", "Tue", "1h", "2026S2", "R2")));

	auto s1 = repo.view_courses_by_semester("2026S1");
	REQUIRE(s1.size() == 1);
	REQUIRE(s1[0].semester == "2026S1");

	REQUIRE(repo.view_courses_by_semester("2099X").empty());
}

TEST(course_view_all) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Mon", "1h", "S1", "R1")));
	REQUIRE(repo.insert_or_replace(make_course("CS102", "t", "B", "i", "Tue", "1h", "S1", "R2")));

	REQUIRE(repo.view_all_courses().size() == 2);
}

TEST(course_view_all_empty) {
	auto repo = make_course_repo();
	REQUIRE(repo.view_all_courses().empty());
}

// ─────────────────────────────────────────────────────────────
// CourseRepository — delete & update
// ─────────────────────────────────────────────────────────────

TEST(course_delete_course) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course()));
	REQUIRE(repo.delete_course("CS101", "A"));
	REQUIRE(repo.view_all_courses().empty());
}

TEST(course_delete_course_cascades_schedules) {
	auto repo = make_course_repo();
	// Insert two schedule rows for the same (code, section)
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Monday",    "1h", "2026S1", "LT1")));
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Wednesday", "1h", "2026S1", "LT1")));
	REQUIRE(repo.search_by_course_code("CS101").size() == 2);

	REQUIRE(repo.delete_course("CS101", "A"));
	REQUIRE(repo.search_by_course_code("CS101").empty());
}

TEST(course_delete_nonexistent_returns_true) {
	// SQLite DELETE on a missing row still returns SQLITE_DONE
	auto repo = make_course_repo();
	REQUIRE(repo.delete_course("NONE", "Z"));
}

TEST(course_update_title_instructor_classroom) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course()));

	Course c = make_course();
	c.title      = "Updated Title";
	c.instructor = "New Prof";
	c.classroom  = "LT99";
	REQUIRE(repo.update(c));

	auto results = repo.search_by_course_code("CS101");
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].title      == "Updated Title");
	REQUIRE(results[0].instructor == "New Prof");
	REQUIRE(results[0].classroom  == "LT99");
}

TEST(course_delete_schedule_leaves_course) {
	auto repo = make_course_repo();
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Monday",    "1h", "2026S1", "LT1")));
	REQUIRE(repo.insert_or_replace(make_course("CS101", "t", "A", "i", "Wednesday", "1h", "2026S1", "LT1")));
	REQUIRE(repo.search_by_course_code("CS101").size() == 2);

	REQUIRE(repo.delete_schedule("CS101", "A", "Monday", "1h", "2026S1"));

	auto results = repo.search_by_course_code("CS101");
	REQUIRE(results.size() == 1);
	REQUIRE(results[0].day == "Wednesday");
}

TEST(course_delete_schedule_nonexistent_returns_true) {
	auto repo = make_course_repo();
	REQUIRE(repo.delete_schedule("NONE", "Z", "Monday", "1h", "2026S1"));
}

// ─────────────────────────────────────────────────────────────
// CourseRepository — empty results for all search variants
// ─────────────────────────────────────────────────────────────

TEST(course_search_all_empty_on_fresh_repo) {
	auto repo = make_course_repo();
	REQUIRE(repo.search_by_course_code("CS101").empty());
	REQUIRE(repo.search_by_instructor("Nobody").empty());
	REQUIRE(repo.search_by_classroom("ZZ99").empty());
	REQUIRE(repo.search_by_day("Sunday").empty());
	REQUIRE(repo.view_courses_by_semester("2099X").empty());
	REQUIRE(repo.view_all_courses().empty());
}

// ─────────────────────────────────────────────────────────────
// AdministratorRepository
// ─────────────────────────────────────────────────────────────

TEST(admin_initialize_schema) {
	AdministratorRepository repo(":memory:");
	REQUIRE(repo.initialize_schema());
}

TEST(admin_initialize_schema_idempotent) {
	AdministratorRepository repo(":memory:");
	REQUIRE(repo.initialize_schema());
	REQUIRE(repo.initialize_schema());
}

TEST(admin_insert_and_verify_login) {
	auto repo = make_admin_repo();
	REQUIRE(repo.insert_or_replace({"admin", "pass123"}));
	REQUIRE(repo.verify_login("admin", "pass123"));
}

TEST(admin_verify_wrong_password) {
	auto repo = make_admin_repo();
	REQUIRE(repo.insert_or_replace({"admin", "pass123"}));
	REQUIRE(!repo.verify_login("admin", "wrongpass"));
}

TEST(admin_verify_nonexistent_user) {
	auto repo = make_admin_repo();
	REQUIRE(!repo.verify_login("ghost", "any"));
}

TEST(admin_insert_conflict_updates_password) {
	auto repo = make_admin_repo();
	REQUIRE(repo.insert_or_replace({"admin", "oldpass"}));
	REQUIRE(repo.insert_or_replace({"admin", "newpass"}));
	REQUIRE(repo.verify_login("admin", "newpass"));
	REQUIRE(!repo.verify_login("admin", "oldpass"));
}

TEST(admin_multiple_users) {
	auto repo = make_admin_repo();
	REQUIRE(repo.insert_or_replace({"alice", "pass_a"}));
	REQUIRE(repo.insert_or_replace({"bob",   "pass_b"}));
	REQUIRE(repo.verify_login("alice", "pass_a"));
	REQUIRE(repo.verify_login("bob",   "pass_b"));
	REQUIRE(!repo.verify_login("alice", "pass_b"));
}

// ─────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────

int main() {
	std::cout << "=== test_database ===\n\n";

	// Database
	std::cout << "-- Database --\n";
	RUN(db_open_memory);
	RUN(db_close_nulls_handle);
	RUN(db_double_close_is_safe);
	RUN(db_open_invalid_path_returns_false);
	RUN(db_reopen_after_close);

	// CourseRepository — schema & insert/query
	std::cout << "\n-- CourseRepository: schema & query --\n";
	RUN(course_initialize_schema);
	RUN(course_initialize_schema_idempotent);
	RUN(course_insert_and_search_by_code);
	RUN(course_insert_conflict_updates_title_and_instructor);
	RUN(course_insert_conflict_updates_classroom);
	RUN(course_search_by_instructor_exact);
	RUN(course_search_by_instructor_partial);
	RUN(course_search_by_classroom_partial);
	RUN(course_search_by_day);
	RUN(course_view_by_semester);
	RUN(course_view_all);
	RUN(course_view_all_empty);

	// CourseRepository — delete & update
	std::cout << "\n-- CourseRepository: delete & update --\n";
	RUN(course_delete_course);
	RUN(course_delete_course_cascades_schedules);
	RUN(course_delete_nonexistent_returns_true);
	RUN(course_update_title_instructor_classroom);
	RUN(course_delete_schedule_leaves_course);
	RUN(course_delete_schedule_nonexistent_returns_true);
	RUN(course_search_all_empty_on_fresh_repo);

	// AdministratorRepository
	std::cout << "\n-- AdministratorRepository --\n";
	RUN(admin_initialize_schema);
	RUN(admin_initialize_schema_idempotent);
	RUN(admin_insert_and_verify_login);
	RUN(admin_verify_wrong_password);
	RUN(admin_verify_nonexistent_user);
	RUN(admin_insert_conflict_updates_password);
	RUN(admin_multiple_users);

	std::cout << "\nResult: " << s_passed << " passed, " << s_failed << " failed.\n";
	return s_failed == 0 ? 0 : 1;
}
