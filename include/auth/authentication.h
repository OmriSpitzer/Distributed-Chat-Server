/**
 * Authentication header file class
 *
 * @date 12-09-2026
 */

#pragma once
#include <string>
#include <string_view>

class Authentication {
public:
  // hash a password (Argon2id encoded string)
  static std::string hashPassword(const std::string_view &password);

  // check if the password matches a stored Argon2id hash
  static bool checkPassword(const std::string_view &password, const std::string_view &hash);
};
