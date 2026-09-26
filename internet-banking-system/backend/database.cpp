#include "database.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

Statement::Statement(sqlite3* db, const std::string& sql) {
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
    if (rc != SQLITE_OK) {
        std::string msg = "Failed to prepare statement: ";
        msg += sqlite3_errmsg(db);
        throw std::runtime_error(msg);
    }
}

Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

void Statement::bindInt(int index, long long value) {
    sqlite3_bind_int64(stmt_, index, value);
}

void Statement::bindText(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

void Statement::bindNull(int index) {
    sqlite3_bind_null(stmt_, index);
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_ROW;
}

bool Statement::execute() {
    int rc = sqlite3_step(stmt_);
    return rc == SQLITE_DONE;
}

long long Statement::columnInt(int index) {
    return sqlite3_column_int64(stmt_, index);
}

std::string Statement::columnText(int index) {
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    if (!text) return "";
    return std::string(reinterpret_cast<const char*>(text));
}

bool Statement::isColumnNull(int index) {
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

Database& Database::instance() {
    static Database db;
    return db;
}

Database::~Database() {
    close();
}

bool Database::open(const std::string& path) {
    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    // Enforce foreign keys and use WAL for better concurrent read/write behavior.
    sqlite3_exec(db_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA busy_timeout = 5000;", nullptr, nullptr, nullptr);
    return true;
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::runSqlFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open SQL file: " << path << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sql = buffer.str();

    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error running " << path << ": " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool Database::begin() {
    return sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::commit() {
    return sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::rollback() {
    return sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr) == SQLITE_OK;
}
