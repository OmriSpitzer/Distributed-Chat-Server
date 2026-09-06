/**
 * Authentication header file class
 *
 * @date 06-09-2026
 */
#pragma once
#include "utils/models/user.h"
#include <string>

class Authentication {
public:
  // register a user
  static User registerUser(const std::string &username, const std::string &password,
                           const std::string &email);

  // login a user
  static User login(const std::string &username, const std::string &password);

  // verify a password
  static bool verifyPassword(const std::string &password);

  // verify an email
  static bool verifyEmail(const std::string &email);

  // verify a username
  static bool verifyUsername(const std::string &username);
};