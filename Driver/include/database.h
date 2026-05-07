#pragma once

#include <string>
#include <vector>

struct sqlite3;

namespace dcn_database {

struct BaseEntity {
	int id = -1;
	virtual ~BaseEntity() = default;
};

struct Course : public BaseEntity {
	std::string code;
	std::string title;
	std::string section;
	std::string instructor;
	std::string day;
	std::string duration;
	std::string classroom;
};

struct Administrator : public BaseEntity {
	std::string username;
	std::string password;
};

class Database {
public:
	Database() = default;
	~Database();

	Database(const Database&) = delete;
	Database& operator=(const Database&) = delete;

	bool open(const std::string& db_path);
	void close();

	const std::string& last_error() const;

	sqlite3* raw_handle() const;

	void set_error(const std::string& message) const;

private:
	sqlite3* db_ = nullptr;
	mutable std::string last_error_;
};

class CourseRepository {
public:
	CourseRepository() = default;
	explicit CourseRepository(const std::string& db_path);

	bool open(const std::string& db_path);
	void close();
	bool initialize_schema() const;

	bool insert_or_replace(const Course& course) const;
	std::vector<Course> search_by_course_code(const std::string& code) const;
	std::vector<Course> search_by_instructor(const std::string& instructor) const;
	std::vector<Course> view_all_courses() const;

	const std::string& last_error() const;

private:
	bool prepare_and_collect(const std::string& sql,
							 const std::vector<std::string>& bindings,
							 std::vector<Course>& out_courses) const;

private:
	mutable Database db_;
};

class AdministratorRepository {
public:
	AdministratorRepository() = default;
	explicit AdministratorRepository(const std::string& db_path);

	bool open(const std::string& db_path);
	void close();
	bool initialize_schema() const;

	bool insert_or_replace(const Administrator& admin) const;
	bool verify_login(const std::string& username, const std::string& password) const;

	const std::string& last_error() const;

private:
	mutable Database db_;
};

}  // namespace dcn_database
