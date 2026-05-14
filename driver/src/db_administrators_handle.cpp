#include "database.h"

#include "sqlite3.h"

namespace dcn_database {

AdministratorRepository::AdministratorRepository(const std::string& db_path) {
	open(db_path);
}

AdministratorRepository::AdministratorRepository() {
	open("data/DCN.db");
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
