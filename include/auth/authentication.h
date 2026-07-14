/**
 * Authentication header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/user.h"
#include <string>

class Authentication {
public:
  // register a user
  static User registerUser(const std::string &username, const std::string &password);

  // login a user
  static User login(const std::string &username, const std::string &password);

  // logout a user
  static void logout(const std::string &username);

  // verify a password
  static bool verifyPassword(const std::string &username, const std::string &password);
};