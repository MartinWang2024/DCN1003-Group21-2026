#pragma once

#include <string>
#include <vector>

struct sqlite3;

namespace dcn_database {

/**
 * @brief Course entity
 */
struct Course {
	/**
	 * @brief Course code
	 */
	std::string code;
	/**
	 * @brief Course title
	 */
	std::string title;
	/**
	 * @brief Section or group
	 */
	std::string section;
	/**
	 * @brief Instructor name
	 */
	std::string instructor;
	/**
	 * @brief Class day
	 */
	std::string day;
	/**
	 * @brief Duration (e.g. "90min")
	 */
	std::string duration;
	/**
	 * @brief Semester identifier
	 */
	std::string semester;
	/**
	 * @brief Classroom
	 */
	std::string classroom;
};

/**
 * @brief Administrator entity
 */
struct Administrator {
	/**
	 * @brief Administrator username
	 */
	std::string username;
	/**
	 * @brief Administrator password (plaintext or PBKDF2-encoded)
	 */
	std::string password;
};

/**
 * @brief SQLite database connection wrapper
 * @details Manages connection lifecycle and stores last error message.
 */
class Database {
public:
	/**
	 * @brief Default constructor
	 */
	Database() = default;
	/**
	 * @brief Automatically closes connection on destruction
	 */
	~Database();

	/**
	 * @brief Copy prohibited — avoids duplicate handle management
	 */
	Database(const Database&) = delete;
	/**
	 * @brief Copy assignment prohibited
	 */
	Database& operator=(const Database&) = delete;

	/**
	 * @brief Move constructor — transfers connection ownership
	 */
	Database(Database&& other) noexcept;
	/**
	 * @brief Move assignment — transfers connection ownership
	 */
	Database& operator=(Database&& other) noexcept;

	/**
	 * @brief Open database connection
	 * @param db_path Path to database file
	 * @return true on success; false and sets error message on failure
	 */
	bool open(const std::string& db_path);
	/**
	 * @brief Close database connection
	 */
	void close();

	/**
	 * @brief Get last error message
	 * @return Reference to last error string
	 */
	const std::string& last_error() const;

	/**
	 * @brief Get raw SQLite connection handle
	 * @return sqlite3* handle; nullptr if not open
	 */
	sqlite3* raw_handle() const;

	/**
	 * @brief Set last error message
	 * @param message Error text
	 */
	void set_error(const std::string& message) const;

private:
	sqlite3* db_ = nullptr;
	mutable std::string last_error_;
};

/**
 * @brief Course data repository
 * @details Schema initialization, insert/update, and queries via JOIN.
 */
class CourseRepository {
public:
	/**
	 * @brief Default constructor
	 */
	CourseRepository();
	/**
	 * @brief Construct and open a specific database
	 * @param db_path Database file path
	 */
	explicit CourseRepository(const std::string& db_path);

	/**
	 * @brief Open database connection
	 * @param db_path Database file path
	 * @return true on success
	 */
	bool open(const std::string& db_path);
	/**
	 * @brief Close database connection
	 */
	void close();
	/**
	 * @brief Initialize course-related tables
	 * @return true on success
	 */
	bool initialize_schema() const;

	/**
	 * @brief Insert or replace (UPSERT) a course record
	 * @param course Course to write
	 * @return true on success
	 */
	bool insert_or_replace(const Course& course) const;
	/**
	 * @brief Search by course code (exact match)
	 * @param code Course code
	 * @return Matching courses; empty on failure/no results
	 */
	std::vector<Course> search_by_course_code(const std::string& code) const;
	/**
	 * @brief Search by instructor name (LIKE match)
	 * @param instructor Instructor name
	 * @return Matching courses; empty on failure/no results
	 */
	std::vector<Course> search_by_instructor(const std::string& instructor) const;
	/**
	 * @brief Search by classroom (LIKE match)
	 * @param classroom Classroom name
	 * @return Matching courses; empty on failure/no results
	 */
	std::vector<Course> search_by_classroom(const std::string& classroom) const;
	/**
	 * @brief Search by day (exact match)
	 * @param day Class day
	 * @return Matching courses; empty on failure/no results
	 */
	std::vector<Course> search_by_day(const std::string& day) const;
	/**
	 * @brief View all courses in a given semester
	 * @param semester Semester identifier
	 * @return Matching courses; empty on failure/no results
	 */
	std::vector<Course> view_courses_by_semester(const std::string& semester) const;
	/**
	 * @brief View all courses (with schedule info)
	 * @return All courses; empty on failure
	 */
	std::vector<Course> view_all_courses() const;

	/**
	 * @brief Paginated view of all courses
	 * @param offset Starting offset (0-based)
	 * @param limit  Max rows to return
	 * @return Page of courses; empty on failure
	 */
	std::vector<Course> view_all_courses_paged(int offset, int limit) const;

	/**
	 * @brief Count total courses
	 * @return Course count; -1 on failure
	 */
	int count_courses() const;

	/**
	 * @brief Delete a course by code and section (CASCADE deletes schedules)
	 * @param code    Course code
	 * @param section Section
	 * @return true on success
	 */
	bool delete_course(const std::string& code, const std::string& section) const;

	/**
	 * @brief Update course title/instructor by (code, section);
	 *        update schedule classroom by (code, section, day, duration, semester)
	 * @param course Course with new field values; key fields identify records
	 * @return true on success
	 */
	bool update(const Course& course) const;
	/**
	 * @brief Delete a specific schedule record (does not affect courses table)
	 * @param code     Course code
	 * @param section  Section
	 * @param day      Class day
	 * @param duration Duration
	 * @param semester Semester
	 * @return true on success
	 */
	bool delete_schedule(const std::string& code, const std::string& section,
						 const std::string& day, const std::string& duration,
						 const std::string& semester) const;

	/**
	 * @brief Get last error message
	 * @return Reference to last error string
	 */
	const std::string& last_error() const;

private:
	/**
	 * @brief Execute query and map result rows to Course objects
	 * @param sql       SQL query
	 * @param bindings  Parameter bindings for prepared statement
	 * @param out_courses Output container
	 * @return true on success
	 */
	bool prepare_and_collect(const std::string& sql,
							 const std::vector<std::string>& bindings,
							 std::vector<Course>& out_courses) const;

private:
	mutable Database db_;
};

/**
 * @brief Administrator data repository
 * @details Schema init, insert/upsert, and login verification.
 */
class AdministratorRepository {
public:
	/**
	 * @brief Default constructor
	 */
	AdministratorRepository();
	/**
	 * @brief Construct and open a specific database
	 * @param db_path Database file path
	 */
	explicit AdministratorRepository(const std::string& db_path);

	/**
	 * @brief Open database connection
	 * @param db_path Database file path
	 * @return true on success
	 */
	bool open(const std::string& db_path);
	/**
	 * @brief Close database connection
	 */
	void close();
	/**
	 * @brief Initialize administrator-related tables
	 * @return true on success
	 */
	bool initialize_schema() const;

	/**
	 * @brief Insert or replace (UPSERT) an admin record; auto-hashes plaintext passwords
	 * @param admin Admin to write
	 * @return true on success
	 */
	bool insert_or_replace(const Administrator& admin) const;
	/**
	 * @brief Verify admin login credentials (PBKDF2 or legacy plaintext)
	 * @param username Username
	 * @param password Password
	 * @return true if credentials match
	 */
	bool verify_login(const std::string& username, const std::string& password) const;

	/**
	 * @brief List all admin usernames (passwords excluded)
	 */
	std::vector<std::string> list_usernames() const;
	/**
	 * @brief Rename administrator (old -> new)
	 * @return true on success; false if old not found, new taken, or DB error
	 */
	bool rename(const std::string& old_username, const std::string& new_username) const;
	/**
	 * @brief Delete administrator account
	 * @return true on success; false if not found or DB error
	 */
	bool remove(const std::string& username) const;
	/**
	 * @brief Check if username exists
	 */
	bool exists(const std::string& username) const;

	/**
	 * @brief Get last error message
	 * @return Reference to last error string
	 */
	const std::string& last_error() const;

private:
	mutable Database db_;
};

}  // namespace dcn_database
