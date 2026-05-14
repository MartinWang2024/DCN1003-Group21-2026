#include "database.h"
#include "test.h"
#include "sqlite3.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

bool table_exists(sqlite3* db, const std::string& table_name) {
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? LIMIT 1;";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_TRANSIENT);
	const int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return rc == SQLITE_ROW;
}

std::vector<std::string> list_tables(sqlite3* db) {
	std::vector<std::string> tables;
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name;";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return tables;
	}

	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const unsigned char* text = sqlite3_column_text(stmt, 0);
		tables.emplace_back(text == nullptr ? "" : reinterpret_cast<const char*>(text));
	}

	sqlite3_finalize(stmt);
	return tables;
}

bool query_count(sqlite3* db, const std::string& table_name, int& out_count) {
	sqlite3_stmt* stmt = nullptr;
	const std::string sql = "SELECT COUNT(*) FROM \"" + table_name + "\";";
	if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	const int rc = sqlite3_step(stmt);
	bool ok = false;
	if (rc == SQLITE_ROW) {
		out_count = sqlite3_column_int(stmt, 0);
		ok = true;
	}
	sqlite3_finalize(stmt);
	return ok;
}

bool query_first_admin_credentials(sqlite3* db, std::vector<std::string>& out_values) {
	sqlite3_stmt* stmt = nullptr;
	const char* sql = "SELECT username, password FROM administrators ORDER BY username LIMIT 1;";
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		return false;
	}

	const int rc = sqlite3_step(stmt);
	if (rc != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		return false;
	}

	out_values.clear();
	out_values.reserve(2);
	for (int i = 0; i < 2; ++i) {
		const unsigned char* text = sqlite3_column_text(stmt, i);
		out_values.emplace_back(text == nullptr ? "" : reinterpret_cast<const char*>(text));
	}

	sqlite3_finalize(stmt);
	return true;
}

}  // namespace

TEST(test_course_db_query_through_database_cpp) {
	dcn_database::Database db;
	REQUIRE(db.open("data/Course.db"));
	sqlite3* handle = db.raw_handle();
	REQUIRE(handle != nullptr);

	auto tables = list_tables(handle);
	std::cout << "Course.db tables:";
	for (const auto& table : tables) {
		std::cout << ' ' << table;
	}
	std::cout << "\n";

	int sqlite_master_count = 0;
	REQUIRE(query_count(handle, "sqlite_master", sqlite_master_count));
	std::cout << "Course.db sqlite_master rows: " << sqlite_master_count << "\n";

	if (table_exists(handle, "courses")) {
		dcn_database::CourseRepository repo;
		REQUIRE(repo.open("data/Course.db"));

		auto courses = repo.view_all_courses();
		std::cout << "Course.db total courses: " << courses.size() << "\n";
		REQUIRE(repo.last_error().empty());

		if (!courses.empty()) {
			const auto first = courses.front();
			auto results = repo.search_by_course_code(first.code);
			REQUIRE(repo.last_error().empty());
			REQUIRE(!results.empty());
			REQUIRE(results.front().code == first.code);
		}
	} else {
		std::cout << "Course.db does not contain a courses table, so repository queries are skipped.\n";
	}
}

TEST(test_admin_db_query_through_database_cpp) {
	dcn_database::Database db;
	REQUIRE(db.open("data/Admin.db"));
	sqlite3* handle = db.raw_handle();
	REQUIRE(handle != nullptr);

	auto tables = list_tables(handle);
	std::cout << "Admin.db tables:";
	for (const auto& table : tables) {
		std::cout << ' ' << table;
	}
	std::cout << "\n";

	int sqlite_master_count = 0;
	REQUIRE(query_count(handle, "sqlite_master", sqlite_master_count));
	std::cout << "Admin.db sqlite_master rows: " << sqlite_master_count << "\n";

	if (table_exists(handle, "administrators")) {
		int admin_count = 0;
		REQUIRE(query_count(handle, "administrators", admin_count));
		std::cout << "Admin.db total administrators: " << admin_count << "\n";

		std::vector<std::string> first_admin;
		if (query_first_admin_credentials(handle, first_admin)) {
			REQUIRE(first_admin.size() == 2);
			std::cout << "First admin user: " << first_admin[0] << "\n";

			dcn_database::AdministratorRepository repo;
			REQUIRE(repo.open("data/Admin.db"));
			auto verified = repo.verify_login(first_admin[0], first_admin[1]);
			REQUIRE(repo.last_error().empty());
			REQUIRE(verified);
		}
	} else {
		std::cout << "Admin.db does not contain an administrators table, so login verification is skipped.\n";
	}
}

int main() {
	std::cout << "=== Real Database Query Test ===\n";
	RUN(test_course_db_query_through_database_cpp);
	RUN(test_admin_db_query_through_database_cpp);

	std::cout << "\n--- Result: " << s_passed << " passed, " << s_failed << " failed ---\n";
	return s_failed == 0 ? 0 : 1;
}


