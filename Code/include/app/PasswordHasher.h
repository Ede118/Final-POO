#pragma once

#include <string>

namespace app {

class PasswordHasher {
public:
  virtual ~PasswordHasher() = default;

  virtual std::string hash(const std::string& plain) = 0;
  virtual bool verify(const std::string& plain, const std::string& hashed) = 0;
};

PasswordHasher& defaultPasswordHasher();

} // namespace app

