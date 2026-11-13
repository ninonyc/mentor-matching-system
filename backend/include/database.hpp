#pragma once
#include "sqlite3.h"
#include <stdexcept>
#include <string>
#include <cstdint>

namespace api {
using Id = std::int64_t;
struct DbError : std::runtime_error {
    int code;
    DbError(int value, const std::string &message) : std::runtime_error(message), code(value) {}
};

// Statements finalize themselves, including during exception unwinding.
class Statement {
    sqlite3 *db_;
    sqlite3_stmt *stmt_ = nullptr;

  public:
    Statement(sqlite3 *db, const std::string &sql) : db_(db) {
        const int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr);
        if (rc != SQLITE_OK) {
            throw DbError(rc, sqlite3_errmsg(db));
        }
    }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement &) = delete;
    Statement &operator=(const Statement &) = delete;
    void bind(int index, Id value) { check(sqlite3_bind_int64(stmt_, index, value)); }
    void bind(int index, const std::string &value) {
        check(sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()),
                                SQLITE_TRANSIENT));
    }
    void bindNull(int index) { check(sqlite3_bind_null(stmt_, index)); }
    bool next() {
        const int rc = sqlite3_step(stmt_);
        if (rc == SQLITE_ROW) {
            return true;
        }
        if (rc == SQLITE_DONE) {
            return false;
        }
        throw DbError(rc, sqlite3_errmsg(db_));
    }
    void execute() {
        if (next()) {
            throw std::runtime_error("Unexpected SQL result");
        }
    }
    Id integer(int index) const { return sqlite3_column_int64(stmt_, index); }
    std::string text(int index) const {
        const auto *value = sqlite3_column_text(stmt_, index);
        return value ? std::string(reinterpret_cast<const char *>(value)) : "";
    }

  private:
    void check(int rc) {
        if (rc != SQLITE_OK) {
            throw DbError(rc, sqlite3_errmsg(db_));
        }
    }
};

class Database {
    sqlite3 *db_ = nullptr;

  public:
    explicit Database(const std::string &path);
    ~Database() { sqlite3_close(db_); }
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
    sqlite3 *handle() { return db_; }
    Statement statement(const std::string &sql) { return Statement(db_, sql); }
    void exec(const std::string &sql);
    Id lastId() const { return sqlite3_last_insert_rowid(db_); }
    Id scalar(const std::string &sql) {
        auto q = statement(sql);
        if (!q.next()) {
            throw std::runtime_error("Missing scalar result");
        }
        return q.integer(0);
    }
};

class Transaction {
    Database &db_;
    bool committed_ = false;

  public:
    explicit Transaction(Database &db) : db_(db) { db_.exec("BEGIN IMMEDIATE"); }
    ~Transaction() {
        if (!committed_) {
            try {
                db_.exec("ROLLBACK");
            } catch (...) {
            }
        }
    }
    void commit() {
        db_.exec("COMMIT");
        committed_ = true;
    }
};
std::string readFile(const std::string &path);
void initialize(Database &db, const std::string &project_root);
} // namespace api
