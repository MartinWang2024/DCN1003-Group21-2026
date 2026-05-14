#include "database.h"

#include "sqlite3.h"

namespace dcn_database {

namespace {

const char* safe_col_text(sqlite3_stmt* stmt, int col) {
	const unsigned char* text = sqlite3_column_text(stmt, col);
	return text == nullptr ? "" : reinterpret_cast<const char*>(text);
}

Course course_from_stmt(sqlite3_stmt* stmt) {
	Course course;
	course.code       = safe_col_text(stmt, 0);
	course.title      = safe_col_text(stmt, 1);
	course.section    = safe_col_text(stmt, 2);
	course.instructor = safe_col_text(stmt, 3);
	course.day        = safe_col_text(stmt, 4);
	course.duration   = safe_col_text(stmt, 5);
	course.semester   = safe_col_text(stmt, 6);
	course.classroom  = safe_col_text(stmt, 7);
	return course;
}

}  // namespace

// 所有查询共用的 SELECT + JOIN 前缀
static const char* kJoinSelect =
	"SELECT c.code, c.title, c.section, c.instructor,"
	"       s.day, s.duration, s.semester, s.classroom"
	" FROM courses c"
	" INNER JOIN schedules s ON c.code = s.course_code AND c.section = s.section";

CourseRepository::CourseRepository(const std::string& db_path) {
	open(db_path);
}

CourseRepository::CourseRepository() {
	open("data/DCN.db");
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
		"  code       VARCHAR(32)  NOT NULL,"
		"  title      VARCHAR(128) NOT NULL,"
		"  section    VARCHAR(32)  NOT NULL,"
		"  instructor VARCHAR(64)  NOT NULL,"
		"  PRIMARY KEY (code, section)"
		");"
		"CREATE TABLE IF NOT EXISTS schedules ("
		"  course_code VARCHAR(32) NOT NULL,"
		"  section     VARCHAR(32) NOT NULL,"
		"  day         DATE        NOT NULL,"
		"  duration    VARCHAR(32) NOT NULL,"
		"  semester    VARCHAR(16) NOT NULL DEFAULT '',"
		"  classroom   VARCHAR(64) NOT NULL DEFAULT '',"
		"  PRIMARY KEY (course_code, section, day, duration, semester),"
		"  FOREIGN KEY (course_code, section)"
		"    REFERENCES courses(code, section)"
		"    ON DELETE CASCADE ON UPDATE CASCADE"
		");"
		"CREATE INDEX IF NOT EXISTS idx_courses_instructor ON courses(instructor);"
		"CREATE INDEX IF NOT EXISTS idx_schedules_classroom ON schedules(classroom);"
		"CREATE INDEX IF NOT EXISTS idx_schedules_day ON schedules(day);"
		"CREATE INDEX IF NOT EXISTS idx_schedules_semester ON schedules(semester);";

	char* err_msg = nullptr;
	const int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err_msg);
	if (rc != SQLITE_OK) {
		db_.set_error(err_msg == nullptr ? "Failed to initialize schema." : err_msg);
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

	char* err_msg = nullptr;
	if (sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
		db_.set_error(err_msg == nullptr ? "Failed to begin transaction." : err_msg);
		sqlite3_free(err_msg);
		return false;
	}

	// 1. 写入 courses 表（冲突时更新 title/instructor）
	const char* courses_sql =
		"INSERT INTO courses(code, title, section, instructor) VALUES(?,?,?,?)"
		" ON CONFLICT(code, section) DO UPDATE SET"
		" title=excluded.title, instructor=excluded.instructor;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, courses_sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	sqlite3_bind_text(stmt, 1, course.code.c_str(),       -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, course.title.c_str(),      -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, course.section.c_str(),    -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, course.instructor.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	// 2. 写入 schedules 表（冲突时更新 classroom）
	const char* schedules_sql =
		"INSERT INTO schedules(course_code, section, day, duration, semester, classroom)"
		" VALUES(?,?,?,?,?,?)"
		" ON CONFLICT(course_code, section, day, duration, semester) DO UPDATE SET"
		" classroom=excluded.classroom;";

	rc = sqlite3_prepare_v2(db, schedules_sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	sqlite3_bind_text(stmt, 1, course.code.c_str(),      -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, course.section.c_str(),   -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, course.day.c_str(),       -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, course.duration.c_str(),  -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, course.semester.c_str(),  -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, course.classroom.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
		db_.set_error(err_msg == nullptr ? "Failed to commit transaction." : err_msg);
		sqlite3_free(err_msg);
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	return true;
}

std::vector<Course> CourseRepository::search_by_course_code(const std::string& code) const {
	std::vector<Course> courses;
	const std::string sql =
		std::string(kJoinSelect) + " WHERE c.code = ? ORDER BY c.section, s.day;";
	prepare_and_collect(sql, {code}, courses);
	return courses;
}

std::vector<Course> CourseRepository::search_by_instructor(const std::string& instructor) const {
	std::vector<Course> courses;
	const std::string sql =
		std::string(kJoinSelect) + " WHERE c.instructor LIKE ? ORDER BY c.code, c.section;";
	prepare_and_collect(sql, {"%" + instructor + "%"}, courses);
	return courses;
}

std::vector<Course> CourseRepository::search_by_classroom(const std::string& classroom) const {
	std::vector<Course> courses;
	const std::string sql =
		std::string(kJoinSelect) + " WHERE s.classroom LIKE ? ORDER BY c.code, c.section;";
	prepare_and_collect(sql, {"%" + classroom + "%"}, courses);
	return courses;
}

std::vector<Course> CourseRepository::search_by_day(const std::string& day) const {
	std::vector<Course> courses;
	const std::string sql =
		std::string(kJoinSelect) + " WHERE s.day = ? ORDER BY c.code, c.section;";
	prepare_and_collect(sql, {day}, courses);
	return courses;
}

std::vector<Course> CourseRepository::view_courses_by_semester(const std::string& semester) const {
	std::vector<Course> courses;
	const std::string sql =
		std::string(kJoinSelect) +
		" WHERE s.semester = ? ORDER BY c.code, c.section, s.day;";
	prepare_and_collect(sql, {semester}, courses);
	return courses;
}

std::vector<Course> CourseRepository::view_all_courses() const {
	std::vector<Course> courses;
	const std::string sql =
		std::string(kJoinSelect) + " ORDER BY c.code, c.section, s.semester, s.day;";
	prepare_and_collect(sql, {}, courses);
	return courses;
}

std::vector<Course> CourseRepository::view_all_courses_paged(int offset, int limit) const {
	std::vector<Course> courses;
	if (limit <= 0) return courses;
	if (offset < 0) offset = 0;
	const std::string sql =
		std::string(kJoinSelect) +
		" ORDER BY c.code, c.section, s.semester, s.day"
		" LIMIT " + std::to_string(limit) +
		" OFFSET " + std::to_string(offset) + ";";
	prepare_and_collect(sql, {}, courses);
	return courses;
}

int CourseRepository::count_courses() const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return 0;
	}
	const char* sql =
		"SELECT COUNT(*) FROM courses c"
		" INNER JOIN schedules s ON c.code = s.course_code AND c.section = s.section;";
	sqlite3_stmt* stmt = nullptr;
	if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return 0;
	}
	int count = 0;
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		count = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return count;
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

	// FK CASCADE 在 open() 时已全局开启，此处直接删除 courses 行即可级联删除 schedules
	const char* sql = "DELETE FROM courses WHERE code = ? AND section = ?;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, code.c_str(),    -1, SQLITE_TRANSIENT);
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

	char* err_msg = nullptr;
	if (sqlite3_exec(db, "BEGIN;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
		db_.set_error(err_msg == nullptr ? "Failed to begin transaction." : err_msg);
		sqlite3_free(err_msg);
		return false;
	}

	// 更新 courses 表中的 title/instructor
	const char* courses_sql =
		"UPDATE courses SET title=?, instructor=? WHERE code=? AND section=?;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, courses_sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	sqlite3_bind_text(stmt, 1, course.title.c_str(),      -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, course.instructor.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, course.code.c_str(),       -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, course.section.c_str(),    -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	// 更新 schedules 表中的 classroom（由完整复合键定位排课记录）
	const char* schedules_sql =
		"UPDATE schedules SET classroom=?"
		" WHERE course_code=? AND section=? AND day=? AND duration=? AND semester=?;";

	rc = sqlite3_prepare_v2(db, schedules_sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	sqlite3_bind_text(stmt, 1, course.classroom.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, course.code.c_str(),      -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, course.section.c_str(),   -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, course.day.c_str(),       -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, course.duration.c_str(),  -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 6, course.semester.c_str(),  -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	if (sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
		db_.set_error(err_msg == nullptr ? "Failed to commit transaction." : err_msg);
		sqlite3_free(err_msg);
		sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
		return false;
	}

	return true;
}

bool CourseRepository::delete_schedule(const std::string& code,
									   const std::string& section,
									   const std::string& day,
									   const std::string& duration,
									   const std::string& semester) const {
	sqlite3* db = db_.raw_handle();
	if (db == nullptr) {
		db_.set_error("Database is not open.");
		return false;
	}

	const char* sql =
		"DELETE FROM schedules"
		" WHERE course_code=? AND section=? AND day=? AND duration=? AND semester=?;";

	sqlite3_stmt* stmt = nullptr;
	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
	if (rc != SQLITE_OK) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	sqlite3_bind_text(stmt, 1, code.c_str(),     -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, section.c_str(),  -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, day.c_str(),      -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, duration.c_str(), -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, semester.c_str(), -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		db_.set_error(sqlite3_errmsg(db));
		return false;
	}

	return true;
}

}  // namespace dcn_database
