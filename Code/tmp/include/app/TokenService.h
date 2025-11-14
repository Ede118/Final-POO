#pragma once

#include <string>

namespace app {

class TokenService {
public:
  virtual ~TokenService() = default;

  virtual std::string issue(size_t bytes = 32) = 0;
};

TokenService& defaultTokenService();

} // namespace app

