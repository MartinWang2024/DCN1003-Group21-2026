#include "database.h"

#include "sqlite3.h"

#include <filesystem>
#include <system_error>

namespace dcn_database {

namespace {

constexpr int kBusyTimeoutMs = 5000;

bool enable_wal_mode(sqlite3* db, std::string& out_error) {
	char* err_msg = nullptr;
	const int rc = sqlite3_exec(db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg);
	if (rc != SQLITE_OK) {
		out_error = err_msg == nullptr ? "Failed to enable WAL mode." : err_msg;
		sqlite3_free(err_msg);
		return false;
	}

	return true;
}

const char* safe_col_text(sqlite3_stmt* stmt, int col) {
	const unsigned char* text = sqlite3_column_text(stmt, col);
	return text == nullptr ? "" : reinterpret_cast<const char*>(text);
}

Course course_from_stmt(sqlite3_stmt* stmt) {
	Course course;
	course.code = safe_col_text(stmt, 0);
	course.title = safe_col_text(stmt, 1);
	course.section = safe_col_text(stmt, 2);
	course.instructor = safe_col_text(stmt, 3);
	course.day = safe_col_text(stmt, 4);
	course.duration = safe_col_text(stmt, 5);
	course.classroom = safe_col_text(stmt, 6);
	return course;
}

}  // namespace

Database::~Database() {
	close();
}

bool Database::open(const std::string& db_path) {
	close();
	// create parent directories if needed (but skip for in-memory and URI paths)
	if (!db_path.empty() && db_path != ":memory:" && db_path.rfind("file:", 0) != 0) {
		std::error_code ec;
		const std::filesystem::path parent = std::filesystem::path(db_path).parent_path();
		if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
			std::filesystem::create_directories(parent, ec);
			if (ec) {
				set_error("Failed to create database directory '" + parent.string() + "': " + ec.message());
				return false;
			}
		}
	}

	const int rc = sqlite3_open_v2(
		db_path.c_str(),
		&db_,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
		nullptr);
	if (rc != SQLITE_OK) {
		if (db_ != nullptr) {
			set_error(sqlite3_errmsg(db_));
		} else {
			set_error(sqlite3_errstr(rc));
		}
		close();
		return false;
	}

	const int busy_timeout_rc = sqlite3_busy_timeout(db_, kBusyTimeoutMs);
	if (busy_timeout_rc != SQLITE_OK) {
		set_error(sqlite3_errmsg(db_));
		close();
		return false;
	}

	std::string wal_error;
	if (!enable_wal_mode(db_, wal_error)) {
		set_error(wal_error);
		close();
		return false;
	}

	return true;
}

void Database::close() {
	if (db_ == nullptr) {
		return;
	}

	sqlite3_close(db_);
	db_ = nullptr;
}

const std::string& Database::last_error() const {
	return last_error_;
}

sqlite3* Database::raw_handle() const {
	return db_;
}

void Database::set_error(const std::string& message) const {
	last_error_ = message;
}

CourseRepository::CourseRepository(const std::string& db_path) {
	open(db_path);
}

CourseRepository::CourseRepository() {
	open("data/courses.db");
}

bool CourseRepository::open(const std::string& db_path) {
	return db_.open(db_path);
}

void CourseRepository::close() {
	db_.close();
}

bool CourseRepository::initialize_schema() const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql =
		"CREATE TABLE IF NOT EXISTS courses ("
		"code TEXT NOT NULL,"
		"title TEXT NOT NULL,"
		"section TEXT NOT NULL,"
		"instructor TEXT NOT NULL,"
		"day TEXT NOT NULL,"
		"duration TEXT NOT NULL,"
		"classroom TEXT NOT NULL,"
		"PRIMARY KEY (code, section)"
		");"
		"CREATE INDEX IF NOT EXISTS idx_courses_instructor ON courses(instructor);";

	char* err_msg = nullptr;
	const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
	if (rc != SQLITE_OK) {
		db_.set_error(err_msg == nullptr ? "Failed to initialize course schema." : err_msg);
		sqlite3_free(err_msg);
		return false;
	}

	return true;
}

const std::string& CourseRepository::last_error() const {
	return db_.last_error();
}

bool CourseRepository::insert_or_replace(const Course& course) const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql =
		"INSERT INTO courses(code, title, section, instructor, day, duration, classroom) "
		"VALUES(?, ?, ?, ?, ?, ?, ?) "
		"ON CONFLICT(code, section) DO UPDATE SET "
		"title=excluded.title,"
		"instructor=excluded.instructor,"
		"day=excluded.day,"
		"duration=excluded.duration,"
		"classroom=excluded.classroom;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, course.code.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, course.title.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, course.section.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, course.instructor.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, course.day.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, course.duration.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, course.classroom.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	return true;
}

std::vector<Course> CourseRepository::search_by_course_code(const std::string& code) const {
	std::vector<Course> courses;
	const std::string sql =
		"SELECT code, title, section, instructor, day, duration, classroom "
		"FROM courses WHERE code = ? ORDER BY section;";

	prepare_and_collect(sql, {code}, courses);
	return courses;
}

std::vector<Course> CourseRepository::search_by_instructor(const std::string& instructor) const {
	std::vector<Course> courses;
	const std::string sql =
		"SELECT code, title, section, instructor, day, duration, classroom "
		"FROM courses WHERE instructor LIKE ? ORDER BY code, section;";

	prepare_and_collect(sql, {"%" + instructor + "%"}, courses);
	return courses;
}

std::vector<Course> CourseRepository::view_all_courses() const {
	std::vector<Course> courses;
	const std::string sql =
		"SELECT code, title, section, instructor, day, duration, classroom "
		"FROM courses ORDER BY code, section;";

	prepare_and_collect(sql, {}, courses);
	return courses;
}

bool CourseRepository::prepare_and_collect(const std::string& sql,
									  const std::vector<std::string>& bindings,
									  std::vector<Course>& out_courses) const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	for (int i = 0; i < static_cast<int>(bindings.size()); ++i) {
		rc = sqlite3_bind_text(stmt, i + 1, bindings[static_cast<std::size_t>(i)].c_str(), -1,
							   SQLITE_TRANSIENT);
		if (rc != SQLITE_OK) {
			db_.set_error(sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			return false;
		}
	}

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		out_courses.push_back(course_from_stmt(stmt));
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	return true;
}

bool CourseRepository::delete_course(const std::string& code, const std::string& section) const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql = "DELETE FROM courses WHERE code = ? AND section = ?;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, section.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	return true;
}

bool CourseRepository::update(const Course& course) const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql =
		"UPDATE courses SET title=?, instructor=?, day=?, duration=?, classroom=? "
		"WHERE code=? AND section=?;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, course.title.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, course.instructor.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, course.day.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, course.duration.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, course.classroom.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, course.code.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 7, course.section.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	return true;
}

AdministratorRepository::AdministratorRepository(const std::string& db_path) {
	open(db_path);
}

AdministratorRepository::AdministratorRepository() {
	open("data/admins.db");
}

bool AdministratorRepository::open(const std::string& db_path) {
	return db_.open(db_path);
}

void AdministratorRepository::close() {
	db_.close();
}

bool AdministratorRepository::initialize_schema() const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql =
		"CREATE TABLE IF NOT EXISTS administrators ("
		"username TEXT NOT NULL PRIMARY KEY,"
		"password TEXT NOT NULL"
		");"
		"CREATE INDEX IF NOT EXISTS idx_admin_username ON administrators(username);";

	char* err_msg = nullptr;
	const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
	if (rc != SQLITE_OK) {
		db_.set_error(err_msg == nullptr ? "Failed to initialize administrator schema." : err_msg);
		sqlite3_free(err_msg);
		return false;
	}

	return true;
}

const std::string& AdministratorRepository::last_error() const {
	return db_.last_error();
}

bool AdministratorRepository::insert_or_replace(const Administrator& admin) const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql =
		"INSERT INTO administrators(username, password) VALUES(?, ?) "
		"ON CONFLICT(username) DO UPDATE SET password=excluded.password;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, admin.username.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, admin.password.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	return true;
}

bool AdministratorRepository::verify_login(const std::string& username,
										   const std::string& password) const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql =
		"SELECT COUNT(*) FROM administrators WHERE username = ? AND password = ?;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_TRANSIENT);

	int matched = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		matched = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);

	return matched > 0;
}

}  // namespace dcn_database
