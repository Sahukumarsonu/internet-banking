#pragma once
#include <sqlite3.h>
#include <string>
#include <mutex>
#include <memory>

// Thin RAII wrapper around a prepared SQLite statement.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    void bindInt(int index, long long value);
    void bindText(int index, const std::string& value);
    void bindNull(int index);

    // Returns true while a row is available (call repeatedly for SELECT).
    bool step();

    long long columnInt(int index);
    std::string columnText(int index);
    bool isColumnNull(int index);

    // Executes an INSERT/UPDATE/DELETE. Returns true on success.
    bool execute();

    sqlite3_stmt* raw() { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

// A tiny connection manager. SQLite handles concurrent reads fine but we
// serialize writes with a mutex, which is more than adequate for a
// college-scale banking demo and keeps the transfer logic simple and safe.
class Database {
public:
    static Database& instance();

    bool open(const std::string& path);
    void close();

    sqlite3* handle() { return db_; }
    std::mutex& writeMutex() { return writeMutex_; }

    // Runs the given .sql file (schema/seed) against the open database.
    bool runSqlFile(const std::string& path);

    // Convenience: begin/commit/rollback a transaction. Caller must hold
    // writeMutex() for the duration when doing multi-statement writes.
    bool begin();
    bool commit();
    bool rollback();

private:
    Database() = default;
    ~Database();
    sqlite3* db_ = nullptr;
    std::mutex writeMutex_;
};
