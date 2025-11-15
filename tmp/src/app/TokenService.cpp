#include "app/TokenService.h"

#include <random>
#include <sstream>
#include <iomanip>
#include <vector>

namespace app {

namespace {

class HexTokenService : public TokenService {
public:
  std::string issue(size_t bytes) override {
    std::vector<unsigned char> buf(bytes);
    std::random_device rd;
    for (auto& b : buf) {
      b = static_cast<unsigned char>(rd());
    }
    std::ostringstream os;
    for (auto b : buf) {
      os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return os.str();
  }
};

} // namespace

TokenService& defaultTokenService() {
  static HexTokenService instance;
  return instance;
}

} // namespace app
