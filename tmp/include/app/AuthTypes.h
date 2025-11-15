#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace app {

enum class AuthStatus {
  Ok = 0,
  InvalidCredentials,
  AccountLocked,
  ResetRequired,
  InternalError
};

struct UserRecord {
  int id{0};
  std::string username;
  std::string passwordHash;
  std::string privilege;
  int failedAttempts{0};
  std::chrono::system_clock::time_point lockedUntil{};
  bool resetRequired{false};
};

struct SessionRecord {
  std::string token;
  int userId{0};
  std::chrono::system_clock::time_point createdAt{};
  std::chrono::system_clock::time_point expiresAt{};
  std::string clientIp;
  std::string userAgent;
};

struct LoginResult {
  AuthStatus status{AuthStatus::InternalError};
  std::string message;
  std::string token;
  std::optional<UserRecord> user;
  std::chrono::system_clock::time_point expiresAt{};
  int retryAfterSeconds{0};
};

} // namespace app

