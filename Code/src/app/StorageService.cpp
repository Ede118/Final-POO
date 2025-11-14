#include "app/StorageService.h"

#include <chrono>
#include <cstring>
#include <iostream>

namespace app {

namespace {
constexpr const char* kPragmaForeignKeys = "PRAGMA foreign_keys=ON;";

sqlite3_stmt* prepare(sqlite3* db, const std::string& sql) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return nullptr;
  }
  return stmt;
}
} // namespace

StorageService::StorageService(std::string dbPath) : dbPath_(std::move(dbPath)) {}

StorageService::~StorageService() {
  close();
}

bool StorageService::open() {
  if (db_) {
    return true;
  }
  if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK) {
    std::cerr << "sqlite3_open fallo: " << sqlite3_errmsg(db_) << "\n";
    close();
    return false;
  }
  exec(kPragmaForeignKeys);
  return true;
}

void StorageService::close() {
  if (db_) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

bool StorageService::initializeSchema() {
  if (!db_ && !open()) {
    return false;
  }

  const char* createUsersSql = R"SQL(
    CREATE TABLE IF NOT EXISTS users (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      username TEXT NOT NULL UNIQUE,
      password_hash TEXT NOT NULL,
      privilege TEXT NOT NULL DEFAULT 'viewer',
      failed_attempts INTEGER NOT NULL DEFAULT 0,
      locked_until INTEGER NOT NULL DEFAULT 0,
      reset_required INTEGER NOT NULL DEFAULT 0,
      last_login INTEGER NOT NULL DEFAULT 0
    );
  )SQL";

  const char* createSessionsSql = R"SQL(
    CREATE TABLE IF NOT EXISTS sessions (
      token TEXT PRIMARY KEY,
      user_id INTEGER NOT NULL,
      created_at INTEGER NOT NULL,
      expires_at INTEGER NOT NULL,
      client_ip TEXT,
      user_agent TEXT,
      FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
    );
  )SQL";

  if (!exec(createUsersSql)) {
    return false;
  }
  if (!exec(createSessionsSql)) {
    return false;
  }
  return true;
}

std::optional<UserRecord> StorageService::findUserByUsername(const std::string& username) {
  if (!db_ && !open()) {
    return std::nullopt;
  }
  const char* sql = R"SQL(
    SELECT id, username, password_hash, privilege, failed_attempts, locked_until, reset_required
    FROM users
    WHERE UPPER(username) = UPPER(?)
    LIMIT 1;
  )SQL";

  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    std::cerr << "prepare findUser fallo: " << sqlite3_errmsg(db_) << "\n";
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);

  UserRecord record;
  int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    record.id = sqlite3_column_int(stmt, 0);
    record.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    record.passwordHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
    record.privilege = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    record.failedAttempts = sqlite3_column_int(stmt, 4);
    auto locked = sqlite3_column_int64(stmt, 5);
    record.lockedUntil = fromUnix(locked);
    record.resetRequired = sqlite3_column_int(stmt, 6) != 0;
    sqlite3_finalize(stmt);
    return record;
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

bool StorageService::resetFailedAttempts(int userId) {
  if (!db_ && !open()) {
    return false;
  }
  const char* sql = R"SQL(
    UPDATE users SET failed_attempts = 0, locked_until = 0 WHERE id = ?;
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return false;
  }
  sqlite3_bind_int(stmt, 1, userId);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool StorageService::recordFailedAttempt(int userId,
                                         int maxFailures,
                                         const std::chrono::seconds& lockDuration,
                                         int& outAttempts,
                                         std::chrono::system_clock::time_point& outLockedUntil) {
  if (!db_ && !open()) {
    return false;
  }

  const char* selectSql = R"SQL(
    SELECT failed_attempts FROM users WHERE id = ?;
  )SQL";
  sqlite3_stmt* selectStmt = prepare(db_, selectSql);
  if (!selectStmt) {
    return false;
  }
  sqlite3_bind_int(selectStmt, 1, userId);
  int rc = sqlite3_step(selectStmt);
  int attempts = 0;
  if (rc == SQLITE_ROW) {
    attempts = sqlite3_column_int(selectStmt, 0);
  }
  sqlite3_finalize(selectStmt);

  attempts += 1;
  std::chrono::system_clock::time_point lockedUntil{};
  if (attempts >= maxFailures) {
    lockedUntil = std::chrono::system_clock::now() + lockDuration;
  }

  const char* updateSql = R"SQL(
    UPDATE users SET failed_attempts = ?, locked_until = ? WHERE id = ?;
  )SQL";
  sqlite3_stmt* updateStmt = prepare(db_, updateSql);
  if (!updateStmt) {
    return false;
  }
  sqlite3_bind_int(updateStmt, 1, attempts);
  sqlite3_bind_int64(updateStmt, 2, toUnix(lockedUntil));
  sqlite3_bind_int(updateStmt, 3, userId);
  rc = sqlite3_step(updateStmt);
  sqlite3_finalize(updateStmt);
  if (rc != SQLITE_DONE) {
    return false;
  }

  outAttempts = attempts;
  outLockedUntil = lockedUntil;
  return true;
}

bool StorageService::markResetRequired(int userId, bool required) {
  if (!db_ && !open()) {
    return false;
  }
  const char* sql = R"SQL(
    UPDATE users SET reset_required = ? WHERE id = ?;
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return false;
  }
  sqlite3_bind_int(stmt, 1, required ? 1 : 0);
  sqlite3_bind_int(stmt, 2, userId);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool StorageService::touchLastLogin(int userId) {
  if (!db_ && !open()) {
    return false;
  }
  const char* sql = R"SQL(
    UPDATE users SET last_login = ? WHERE id = ?;
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return false;
  }
  sqlite3_bind_int64(stmt, 1, toUnix(std::chrono::system_clock::now()));
  sqlite3_bind_int(stmt, 2, userId);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool StorageService::createSession(const SessionRecord& session) {
  if (!db_ && !open()) {
    return false;
  }
  const char* sql = R"SQL(
    INSERT OR REPLACE INTO sessions
    (token, user_id, created_at, expires_at, client_ip, user_agent)
    VALUES (?, ?, ?, ?, ?, ?);
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, session.token.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 2, session.userId);
  sqlite3_bind_int64(stmt, 3, toUnix(session.createdAt));
  sqlite3_bind_int64(stmt, 4, toUnix(session.expiresAt));
  sqlite3_bind_text(stmt, 5, session.clientIp.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 6, session.userAgent.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

std::optional<SessionRecord> StorageService::findSession(const std::string& token) {
  if (!db_ && !open()) {
    return std::nullopt;
  }
  const char* sql = R"SQL(
    SELECT token, user_id, created_at, expires_at, client_ip, user_agent
    FROM sessions WHERE token = ? LIMIT 1;
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    SessionRecord rec;
    rec.token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    rec.userId = sqlite3_column_int(stmt, 1);
    rec.createdAt = fromUnix(sqlite3_column_int64(stmt, 2));
    rec.expiresAt = fromUnix(sqlite3_column_int64(stmt, 3));
    const unsigned char* ip = sqlite3_column_text(stmt, 4);
    const unsigned char* ua = sqlite3_column_text(stmt, 5);
    rec.clientIp = ip ? reinterpret_cast<const char*>(ip) : "";
    rec.userAgent = ua ? reinterpret_cast<const char*>(ua) : "";
    sqlite3_finalize(stmt);
    return rec;
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

bool StorageService::deleteSession(const std::string& token) {
  if (!db_ && !open()) {
    return false;
  }
  const char* sql = R"SQL(
    DELETE FROM sessions WHERE token = ?;
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return false;
  }
  sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool StorageService::updateSessionExpiry(const std::string& token,
                                         std::chrono::system_clock::time_point expiresAt) {
  if (!db_ && !open()) {
    return false;
  }
  const char* sql = R"SQL(
    UPDATE sessions SET expires_at = ? WHERE token = ?;
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return false;
  }
  sqlite3_bind_int64(stmt, 1, toUnix(expiresAt));
  sqlite3_bind_text(stmt, 2, token.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool StorageService::purgeExpiredSessions() {
  if (!db_ && !open()) {
    return false;
  }
  const char* sql = R"SQL(
    DELETE FROM sessions WHERE expires_at <= ?;
  )SQL";
  sqlite3_stmt* stmt = prepare(db_, sql);
  if (!stmt) {
    return false;
  }
  sqlite3_bind_int64(stmt, 1, toUnix(std::chrono::system_clock::now()));
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return rc == SQLITE_DONE;
}

bool StorageService::exec(const std::string& sql) {
  char* err = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    if (err) {
      std::cerr << "sqlite3_exec error: " << err << "\n";
      sqlite3_free(err);
    }
    return false;
  }
  return true;
}

std::chrono::system_clock::time_point StorageService::fromUnix(sqlite3_int64 value) {
  return std::chrono::system_clock::time_point{std::chrono::seconds(value)};
}

sqlite3_int64 StorageService::toUnix(std::chrono::system_clock::time_point tp) {
  return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

} // namespace app
