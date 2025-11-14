#pragma once

#include "app/AuthTypes.h"
#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include "thirdparty/sqlite3_stub.h"

namespace app {

class StorageService {
public:
  explicit StorageService(std::string dbPath);
  ~StorageService();

  StorageService(const StorageService&) = delete;
  StorageService& operator=(const StorageService&) = delete;

  bool open();
  void close();
  bool initializeSchema();

  std::optional<UserRecord> findUserByUsername(const std::string& username);
  bool resetFailedAttempts(int userId);
  bool recordFailedAttempt(int userId, int maxFailures, const std::chrono::seconds& lockDuration,
                           int& outAttempts, std::chrono::system_clock::time_point& outLockedUntil);
  bool markResetRequired(int userId, bool required);

  bool touchLastLogin(int userId);

  bool createSession(const SessionRecord& session);
  std::optional<SessionRecord> findSession(const std::string& token);
  bool deleteSession(const std::string& token);
  bool updateSessionExpiry(const std::string& token,
                           std::chrono::system_clock::time_point expiresAt);
  bool purgeExpiredSessions();

  const std::string& path() const { return dbPath_; }

private:
  std::string dbPath_;
  sqlite3* db_{nullptr};

  bool exec(const std::string& sql);
  static std::chrono::system_clock::time_point fromUnix(sqlite3_int64 value);
  static sqlite3_int64 toUnix(std::chrono::system_clock::time_point tp);
};

} // namespace app
