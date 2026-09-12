/**
 * Authentication class
 *
 * @brief System authentication methods.
 * @date 12-09-2026
 */

#include "auth/authentication.h"
#include "argon2.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// hash constants
namespace {
constexpr std::size_t kSaltLen = 16;
constexpr std::size_t kHashLen = 32;
constexpr uint32_t kTCost = 2;
constexpr uint32_t kMCost = 1 << 16;
constexpr uint32_t kParallelism = 1;
} // namespace

// hash a password
std::string Authentication::hashPassword(const std::string_view &password) {
  std::vector<uint8_t> salt(kSaltLen);
  std::random_device rd;
  for (auto &b : salt) {
    b = static_cast<uint8_t>(rd());
  }

  const auto encodedLen =
      argon2_encodedlen(kTCost, kMCost, kParallelism, kSaltLen, kHashLen, Argon2_id);
  std::string encoded(encodedLen, '\0');
  const int rc =
      argon2id_hash_encoded(kTCost, kMCost, kParallelism, password.data(), password.size(),
                            salt.data(), salt.size(), kHashLen, encoded.data(), encoded.size());
  if (rc != ARGON2_OK) {
    throw std::runtime_error(argon2_error_message(rc));
  }
  encoded.resize(std::strlen(encoded.c_str()));
  return encoded;
}

// check if the password is correct
bool Authentication::checkPassword(const std::string_view &password, const std::string_view &hash) {
  const std::string encoded(hash);
  const int rc = argon2id_verify(encoded.c_str(), password.data(), password.size());
  if (rc == ARGON2_OK) {
    return true;
  }
  // seed / pre-hash rows store plaintext; only accept those when the value is not an Argon2 string
  if (rc == ARGON2_VERIFY_MISMATCH) {
    return false;
  }
  return encoded == password;
}