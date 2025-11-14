#include "app/Logger.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <ctime>

namespace app {

namespace {
std::mutex& logMutex() {
  static std::mutex m;
  return m;
}

std::string nowString() {
  auto now = std::chrono::system_clock::now();
  std::time_t tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  localtime_s(&tm, &tt);
#else
  localtime_r(&tt, &tm);
#endif
  std::ostringstream os;
  os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return os.str();
}
} // namespace

Logger::Logger(std::string logPath) : logPath_(std::move(logPath)) {}

void Logger::logAuthSuccess(const std::string& username, const std::string& clientIp) {
  write("INFO", "login ok user=" + username + " ip=" + clientIp);
}

void Logger::logAuthFailure(const std::string& username,
                            const std::string& clientIp,
                            const std::string& reason) {
  write("WARN", "login fail user=" + username + " ip=" + clientIp + " reason=" + reason);
}

void Logger::logSecurityEvent(const std::string& message) {
  write("SECURITY", message);
}

void Logger::write(const std::string& level, const std::string& message) {
  std::lock_guard<std::mutex> guard(logMutex());
  std::ofstream out(logPath_, std::ios::app);
  if (!out.is_open()) {
    return;
  }
  out << nowString() << " [" << level << "] " << message << "\n";
}

} // namespace app
