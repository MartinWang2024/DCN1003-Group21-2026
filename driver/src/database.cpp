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

}  // namespace

Database::~Database() {
	close();
}

Database::Database(Database&& other) noexcept
	: db_(other.db_), last_error_(std::move(other.last_error_)) {
	other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
	if (this != &other) {
		close();
		db_         = other.db_;
		last_error_ = std::move(other.last_error_);
		other.db_   = nullptr;
	}
	return *this;
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

	// 每个连接都需要单独开启 FK 约束（SQLite 默认关闭）
	char* fk_err = nullptr;
	const int fk_rc = sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, &fk_err);
	if (fk_rc != SQLITE_OK) {
		const std::string msg = fk_err == nullptr ? "Failed to enable foreign keys." : fk_err;
		sqlite3_free(fk_err);
		set_error(msg);
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

}  // namespace dcn_database
