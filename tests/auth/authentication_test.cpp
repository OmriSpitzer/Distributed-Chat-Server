/**
 * Authentication unit tests
 *
 * @brief Includes: Argon2id encoding, unique salts, verify match / mismatch, empty /
 * long / unicode / whitespace / embedded-null passwords, plaintext fallback, malformed
 * hash, concurrent hash/check, typical register/login flow.
 * @date 13-09-2026
 */

#include "auth/authentication.h"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

/**
 * 1. hashPassword encodes Argon2id
 * 2. same password produces distinct hashes
 * 3. checkPassword accepts the matching password
 * 4. checkPassword rejects a wrong password
 * 5. empty password
 * 6. long password
 * 7. unicode and special characters
 * 8. whitespace password
 * 9. embedded null bytes
 * 10. plaintext fallback when stored value is not Argon2
 * 11. malformed Argon2 string
 * 12. empty stored hash
 * 13. concurrent hash / check
 * 14. typical register / login flow
 */

namespace {

bool isArgon2id(std::string_view encoded) {
  return encoded.find("$argon2id$") == 0 && encoded.find("$m=65536,t=2,p=1$") != std::string::npos;
}

} // namespace

// 1. hashPassword encodes Argon2id
TEST_CASE("Authentication hashPassword encodes Argon2id", "[authentication][hash]") {
  const std::string encoded = Authentication::hashPassword("secret");

  REQUIRE_FALSE(encoded.empty());
  REQUIRE(encoded != "secret");
  REQUIRE(isArgon2id(encoded));
  REQUIRE(encoded.find('\0') == std::string::npos);
}

// 2. same password produces distinct hashes
TEST_CASE("Authentication hashPassword uses a unique salt", "[authentication][hash][edge]") {
  const std::string first = Authentication::hashPassword("same-password");
  const std::string second = Authentication::hashPassword("same-password");

  REQUIRE(isArgon2id(first));
  REQUIRE(isArgon2id(second));
  REQUIRE(first != second);
  REQUIRE(Authentication::checkPassword("same-password", first));
  REQUIRE(Authentication::checkPassword("same-password", second));
}

// 3. checkPassword accepts the matching password
TEST_CASE("Authentication checkPassword accepts the matching password",
          "[authentication][check]") {
  const std::string encoded = Authentication::hashPassword("correct-horse");
  REQUIRE(Authentication::checkPassword("correct-horse", encoded));
}

// 4. checkPassword rejects a wrong password
TEST_CASE("Authentication checkPassword rejects a wrong password",
          "[authentication][check][edge]") {
  const std::string encoded = Authentication::hashPassword("correct-horse");

  REQUIRE_FALSE(Authentication::checkPassword("correct-horse ", encoded));
  REQUIRE_FALSE(Authentication::checkPassword("Correct-horse", encoded));
  REQUIRE_FALSE(Authentication::checkPassword("", encoded));
  REQUIRE_FALSE(Authentication::checkPassword("other", encoded));
}

// 5. empty password
TEST_CASE("Authentication empty password hashes and verifies", "[authentication][hash][edge]") {
  const std::string encoded = Authentication::hashPassword("");

  REQUIRE(isArgon2id(encoded));
  REQUIRE(Authentication::checkPassword("", encoded));
  REQUIRE_FALSE(Authentication::checkPassword("x", encoded));
}

// 6. long password
TEST_CASE("Authentication long password hashes and verifies", "[authentication][hash][edge]") {
  const std::string password(4096, 'a');
  const std::string encoded = Authentication::hashPassword(password);

  REQUIRE(isArgon2id(encoded));
  REQUIRE(Authentication::checkPassword(password, encoded));
  REQUIRE_FALSE(Authentication::checkPassword(std::string(4095, 'a'), encoded));
}

// 7. unicode and special characters
TEST_CASE("Authentication unicode password hashes and verifies", "[authentication][hash][edge]") {
  constexpr std::string_view password = "päss wörd \xF0\x9F\x94\x90 !@#";
  const std::string encoded = Authentication::hashPassword(password);

  REQUIRE(isArgon2id(encoded));
  REQUIRE(Authentication::checkPassword(password, encoded));
  REQUIRE_FALSE(Authentication::checkPassword("pass word", encoded));
}

// 8. whitespace password
TEST_CASE("Authentication whitespace password is significant", "[authentication][hash][edge]") {
  const std::string encoded = Authentication::hashPassword("  secret  ");

  REQUIRE(Authentication::checkPassword("  secret  ", encoded));
  REQUIRE_FALSE(Authentication::checkPassword("secret", encoded));
  REQUIRE_FALSE(Authentication::checkPassword("  secret", encoded));
}

// 9. embedded null bytes
TEST_CASE("Authentication password with embedded null bytes", "[authentication][hash][edge]") {
  const std::string password("pre\0post", 8);
  const std::string encoded = Authentication::hashPassword(password);

  REQUIRE(isArgon2id(encoded));
  REQUIRE(Authentication::checkPassword(password, encoded));
  REQUIRE_FALSE(Authentication::checkPassword("pre", encoded));
  REQUIRE_FALSE(Authentication::checkPassword(std::string("pre\0POST", 8), encoded));
}

// 10. plaintext fallback when stored value is not Argon2
TEST_CASE("Authentication checkPassword falls back to plaintext compare",
          "[authentication][check][legacy]") {
  REQUIRE(Authentication::checkPassword("seed-password", "seed-password"));
  REQUIRE_FALSE(Authentication::checkPassword("seed-password", "other"));
  REQUIRE_FALSE(Authentication::checkPassword("seed-password", "Seed-password"));
}

// 11. malformed Argon2 string
TEST_CASE("Authentication malformed Argon2 string uses plaintext compare",
          "[authentication][check][edge]") {
  constexpr std::string_view bogus = "$argon2id$not-a-real-hash";

  REQUIRE(Authentication::checkPassword(bogus, bogus));
  REQUIRE_FALSE(Authentication::checkPassword("secret", bogus));
  REQUIRE_FALSE(Authentication::checkPassword("secret", "$argon2id$v=19$m=65536,t=2,p=1$"));
}

// 12. empty stored hash
TEST_CASE("Authentication empty stored hash", "[authentication][check][edge]") {
  REQUIRE(Authentication::checkPassword("", ""));
  REQUIRE_FALSE(Authentication::checkPassword("secret", ""));
  REQUIRE_FALSE(Authentication::checkPassword("", "secret"));
}

// 13. concurrent hash / check
TEST_CASE("Authentication concurrent hash and check", "[authentication][thread]") {
  constexpr int kWorkers = 4;
  std::vector<std::string> hashes(static_cast<std::size_t>(kWorkers));
  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(kWorkers));

  for (int i = 0; i < kWorkers; ++i) {
    threads.emplace_back([i, &hashes] {
      const std::string password = "pw-" + std::to_string(i);
      hashes[static_cast<std::size_t>(i)] = Authentication::hashPassword(password);
    });
  }
  for (std::thread &thread : threads) {
    thread.join();
  }

  for (int i = 0; i < kWorkers; ++i) {
    const std::string password = "pw-" + std::to_string(i);
    REQUIRE(isArgon2id(hashes[static_cast<std::size_t>(i)]));
    REQUIRE(Authentication::checkPassword(password, hashes[static_cast<std::size_t>(i)]));
    REQUIRE_FALSE(Authentication::checkPassword("other", hashes[static_cast<std::size_t>(i)]));
  }
}

// 14. typical register / login flow
TEST_CASE("Authentication typical register login flow", "[authentication][flow]") {
  const std::string password = "user-secret";
  const std::string stored = Authentication::hashPassword(password);

  REQUIRE(isArgon2id(stored));
  REQUIRE(Authentication::checkPassword(password, stored));
  REQUIRE_FALSE(Authentication::checkPassword("wrong-secret", stored));

  const std::string rotated = Authentication::hashPassword(password);
  REQUIRE(rotated != stored);
  REQUIRE(Authentication::checkPassword(password, rotated));
  REQUIRE(Authentication::checkPassword(password, stored));
}
