#pragma once

#include <string>

namespace app {

class Logger {
public:
  explicit Logger(std::string logPath);

  void logAuthSuccess(const std::string& username, const std::string& clientIp);
  void logAuthFailure(const std::string& username,
                      const std::string& clientIp,
                      const std::string& reason);
  void logSecurityEvent(const std::string& message);

  const std::string& path() const { return logPath_; }

private:
  std::string logPath_;

  void write(const std::string& level, const std::string& message);
};

} // namespace app
