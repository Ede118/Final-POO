#include "app/AuthService.h"
#include "app/Logger.h"
#include "app/PasswordHasher.h"
#include "app/StorageService.h"
#include "app/TokenService.h"

#include <chrono>

namespace app {

AuthService::AuthService(StorageService& storage,
                         Logger& logger,
                         PasswordHasher& hasher,
                         TokenService& tokens)
    : storage_(storage),
      logger_(logger),
      hasher_(hasher),
      tokens_(tokens) {}

LoginResult AuthService::login(const std::string& username,
                               const std::string& password,
                               const std::string& clientIp,
                               const std::string& userAgent) {
  auto userOpt = storage_.findUserByUsername(username);
  if (!userOpt) {
    logger_.logAuthFailure(username, clientIp, "user_not_found");
    return makeResult(AuthStatus::InvalidCredentials, "INVALID_CREDENTIALS");
  }

  UserRecord user = *userOpt;
  auto now = std::chrono::system_clock::now();
  if (user.lockedUntil > now) {
    auto retry = std::chrono::duration_cast<std::chrono::seconds>(user.lockedUntil - now).count();
    auto res = makeResult(AuthStatus::AccountLocked, "ACCOUNT_LOCKED", user);
    res.retryAfterSeconds = static_cast<int>(retry);
    return res;
  }

  if (user.resetRequired) {
    return makeResult(AuthStatus::ResetRequired, "RESET_REQUIRED", user);
  }

  if (!hasher_.verify(password, user.passwordHash)) {
    int attempts = 0;
    std::chrono::system_clock::time_point lockedUntil{};
    if (!storage_.recordFailedAttempt(user.id, maxFailures_, lockDuration_, attempts, lockedUntil)) {
      logger_.logSecurityEvent("record_failed_attempt_failed user=" + user.username);
      return makeResult(AuthStatus::InternalError, "INTERNAL_ERROR");
    }
    if (lockedUntil > now) {
      auto retry = std::chrono::duration_cast<std::chrono::seconds>(lockedUntil - now).count();
      logger_.logAuthFailure(user.username, clientIp, "account_locked");
      auto res = makeResult(AuthStatus::AccountLocked, "ACCOUNT_LOCKED", user);
      res.retryAfterSeconds = static_cast<int>(retry);
      return res;
    }
    logger_.logAuthFailure(user.username, clientIp, "invalid_credentials");
    return makeResult(AuthStatus::InvalidCredentials, "INVALID_CREDENTIALS", user);
  }

  if (!storage_.resetFailedAttempts(user.id)) {
    logger_.logSecurityEvent("reset_failed_attempts_failed user=" + user.username);
    return makeResult(AuthStatus::InternalError, "INTERNAL_ERROR");
  }

  storage_.touchLastLogin(user.id);

  SessionRecord session;
  session.token = tokens_.issue();
  session.userId = user.id;
  session.createdAt = now;
  session.expiresAt = now + defaultTtl_;
  session.clientIp = clientIp;
  session.userAgent = userAgent;

  if (!storage_.createSession(session)) {
    logger_.logSecurityEvent("persist_session_failed user=" + user.username);
    return makeResult(AuthStatus::InternalError, "INTERNAL_ERROR");
  }

  logger_.logAuthSuccess(user.username, clientIp);

  auto res = makeResult(AuthStatus::Ok, "OK", user, session.token, session.expiresAt);
  return res;
}

bool AuthService::validateToken(const std::string& token, SessionRecord* outSession) {
  auto session = storage_.findSession(token);
  if (!session) {
    return false;
  }
  auto now = std::chrono::system_clock::now();
  if (session->expiresAt <= now) {
    storage_.deleteSession(token);
    return false;
  }
  if (outSession) {
    *outSession = *session;
  }
  return true;
}

bool AuthService::revokeToken(const std::string& token) {
  return storage_.deleteSession(token);
}

bool AuthService::refreshToken(const std::string& token,
                               const std::chrono::seconds& ttl,
                               SessionRecord* updated) {
  auto session = storage_.findSession(token);
  if (!session) {
    return false;
  }
  auto now = std::chrono::system_clock::now();
  if (session->expiresAt <= now) {
    storage_.deleteSession(token);
    return false;
  }
  session->expiresAt = now + ttl;
  if (!storage_.updateSessionExpiry(token, session->expiresAt)) {
    return false;
  }
  if (updated) {
    *updated = *session;
  }
  return true;
}

LoginResult AuthService::makeResult(AuthStatus status,
                                    const std::string& message,
                                    const std::optional<UserRecord>& user,
                                    const std::string& token,
                                    std::chrono::system_clock::time_point expires) {
  LoginResult result;
  result.status = status;
  result.message = message;
  result.token = token;
  result.expiresAt = expires;
  if (user) {
    result.user = user;
  }
  return result;
}

} // namespace app
