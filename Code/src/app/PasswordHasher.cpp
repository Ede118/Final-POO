#include "app/PasswordHasher.h"

#include "thirdparty/openssl_stub.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <cstring>

namespace app {

namespace {

constexpr int kIterations = 120000;
constexpr size_t kSaltBytes = 16;
constexpr size_t kKeyBytes = 32;

std::string toHex(const std::vector<unsigned char>& data) {
  std::ostringstream os;
  for (auto b : data) {
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
  }
  return os.str();
}

std::vector<unsigned char> fromHex(std::string_view hex) {
  if (hex.size() % 2 != 0) {
    throw std::runtime_error("invalid hex length");
  }
  std::vector<unsigned char> out;
  out.reserve(hex.size() / 2);
  for (size_t i = 0; i < hex.size(); i += 2) {
    unsigned int byte = 0;
    std::istringstream iss(std::string(hex.substr(i, 2)));
    iss >> std::hex >> byte;
    out.push_back(static_cast<unsigned char>(byte));
  }
  return out;
}

class Pbkdf2PasswordHasher : public PasswordHasher {
public:
  std::string hash(const std::string& plain) override {
    std::vector<unsigned char> salt(kSaltBytes);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) != 1) {
      throw std::runtime_error("RAND_bytes failed");
    }

    std::vector<unsigned char> key(kKeyBytes);
    if (PKCS5_PBKDF2_HMAC(plain.c_str(), static_cast<int>(plain.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          kIterations, EVP_sha256(),
                          static_cast<int>(key.size()), key.data()) != 1) {
      throw std::runtime_error("PBKDF2 failed");
    }

    std::ostringstream os;
    os << "pbkdf2$" << kIterations << "$" << toHex(salt) << "$" << toHex(key);
    return os.str();
  }

  bool verify(const std::string& plain, const std::string& hashed) override {
    const std::string prefix = "pbkdf2$";
    if (hashed.compare(0, prefix.size(), prefix) != 0) {
      return false;
    }
    auto rest = hashed.substr(prefix.size());
    auto iterPos = rest.find('$');
    if (iterPos == std::string::npos) {
      return false;
    }
    int iterations = std::stoi(rest.substr(0, iterPos));
    auto saltAndKey = rest.substr(iterPos + 1);
    auto saltPos = saltAndKey.find('$');
    if (saltPos == std::string::npos) {
      return false;
    }
    auto saltHex = saltAndKey.substr(0, saltPos);
    auto keyHex = saltAndKey.substr(saltPos + 1);

    std::vector<unsigned char> salt;
    std::vector<unsigned char> expected;
    try {
      salt = fromHex(saltHex);
      expected = fromHex(keyHex);
    } catch (...) {
      return false;
    }

    std::vector<unsigned char> derived(expected.size());
    if (PKCS5_PBKDF2_HMAC(plain.c_str(), static_cast<int>(plain.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          iterations, EVP_sha256(),
                          static_cast<int>(derived.size()), derived.data()) != 1) {
      return false;
    }

    return CRYPTO_memcmp(derived.data(), expected.data(), derived.size()) == 0;
  }
};

} // namespace

PasswordHasher& defaultPasswordHasher() {
  static Pbkdf2PasswordHasher hasher;
  return hasher;
}

} // namespace app
