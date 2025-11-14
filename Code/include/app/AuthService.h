#pragma once

#include "app/AuthTypes.h"
#include <chrono>
#include <string>

namespace app {

class StorageService;
class Logger;
class PasswordHasher;
class TokenService;

class AuthService {
public:
  AuthService(StorageService& storage,
              Logger& logger,
              PasswordHasher& hasher,
              TokenService& tokens);

  LoginResult login(const std::string& username,
                    const std::string& password,
                    const std::string& clientIp,
                    const std::string& userAgent);

  bool validateToken(const std::string& token, SessionRecord* outSession = nullptr);
  bool revokeToken(const std::string& token);
  bool refreshToken(const std::string& token,
                    const std::chrono::seconds& ttl,
                    SessionRecord* updated = nullptr);

private:
  StorageService& storage_;
  Logger& logger_;
  PasswordHasher& hasher_;
  TokenService& tokens_;
  const int maxFailures_{5};
  const std::chrono::seconds lockDuration_{std::chrono::minutes(5)};
  const std::chrono::seconds defaultTtl_{std::chrono::hours(8)};

  LoginResult makeResult(AuthStatus status,
                         const std::string& message,
                         const std::optional<UserRecord>& user = std::nullopt,
                         const std::string& token = std::string{},
                         std::chrono::system_clock::time_point expires = {});
};

} // namespace app
