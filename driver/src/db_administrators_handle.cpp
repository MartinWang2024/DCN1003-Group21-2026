#include "database.h"
#include "password_hash.h"

#include "sqlite3.h"

namespace dcn_database {

namespace {

bool fetch_password(sqlite3* db, const std::string& username,
                    std::string& out) {
    const char* sql = "SELECT password FROM administrators WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* p = sqlite3_column_text(stmt, 0);
        if (p != nullptr) {
            out.assign(reinterpret_cast<const char*>(p));
            found = true;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}

bool write_password(sqlite3* db, const std::string& username,
                    const std::string& encoded) {
    const char* sql = "UPDATE administrators SET password = ? WHERE username = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, encoded.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

}  // namespace

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

	// 写入前若是明文则统一转为 PBKDF2 编码; 已编码值原样写入(支持外部预哈希)
	std::string stored = PasswordHash::is_encoded(admin.password)
	                         ? admin.password
	                         : PasswordHash::make(admin.password);
	if (stored.empty()) {
		db_.set_error("Failed to hash password.");
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
	sqlite3_bind_text(stmt, 2, stored.c_str(),         -1, SQLITE_TRANSIENT);

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

	std::string stored;
	if (!fetch_password(db, username, stored)) {
		return false;
	}

	if (PasswordHash::is_encoded(stored)) {
		return PasswordHash::verify(password, stored);
	}

	// 兼容历史明文记录: 命中后立即原地升级为 PBKDF2 编码
	if (stored == password) {
		std::string upgraded = PasswordHash::make(password);
		if (!upgraded.empty()) {
			write_password(db, username, upgraded);
		}
		return true;
	}

	return false;
}

}  // namespace dcn_database
