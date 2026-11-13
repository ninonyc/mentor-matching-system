#include "database.hpp"
#include <fstream>
#include <sstream>

namespace api {
Database::Database(const std::string &path) {
    const int rc =
        sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "Could not open database";
        sqlite3_close(db_);
        db_ = nullptr;
        throw DbError(rc, message);
    }
    try {
        exec("PRAGMA foreign_keys=ON; PRAGMA recursive_triggers=ON; PRAGMA busy_timeout=5000;");
        if (scalar("PRAGMA foreign_keys") != 1 || scalar("PRAGMA recursive_triggers") != 1) {
            throw std::runtime_error("Required SQLite guards are disabled");
        }
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}
void Database::exec(const std::string &sql) {
    const int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        throw DbError(rc, sqlite3_errmsg(db_));
    }
}
std::string readFile(const std::string &path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Cannot read migration: " + path);
    }
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}
void initialize(Database &db, const std::string &root) {
    Id version = db.scalar("PRAGMA user_version");
    if (version == 0) {
        if (db.scalar("SELECT count(*) FROM sqlite_master WHERE type='table' AND name NOT LIKE "
                      "'sqlite_%'") != 0) {
            throw std::runtime_error("Refusing initialization of an unversioned nonempty database");
        }
        db.exec(readFile(root + "/database/migrations/001_initial.sql"));
        version = 1;
    }
    if (version == 1) {
        db.exec(readFile(root + "/database/migrations/002_auth.sql"));
        version = 2;
    }
    if (version != 2) {
        throw std::runtime_error("Unsupported database schema version");
    }
}
} // namespace api
