/**
 * Authentication class
 *
 * @brief Basic username/password authentication backed by the text database.
 * @date 06-09-2026
 */

#include "auth/authentication.h"
#include "utils/models/user.h"
#include <string>

// register a user
User Authentication::registerUser(const std::string &username, const std::string &password,
                                  const std::string &email) {
  if (!verifyUsername(username)) {
    throw std::invalid_argument("Invalid username");
  }
  if (!verifyPassword(password)) {
    throw std::invalid_argument("Invalid password");
  }
  if (!verifyEmail(email)) {
    throw std::invalid_argument("Invalid email");
  }
  return User(username, email, User::UserType::USER);
}

// login a user
User Authentication::login(const std::string &username, const std::string &password) {
  if (!verifyUsername(username)) {
    throw std::invalid_argument("Invalid username");
  }
  if (!verifyPassword(password)) {
    throw std::invalid_argument("Invalid password");
  }
  return User(username, "", User::UserType::USER);
}

// verify a password
bool Authentication::verifyPassword(const std::string &password) {
  if (password.empty()) {
    return false;
  }
  return true;
}

// verify an email
bool Authentication::verifyEmail(const std::string &email) {
  if (email.empty()) {
    return false;
  }
  return true;
}

// verify a username
bool Authentication::verifyUsername(const std::string &username) {
  if (username.empty()) {
    return false;
  }
  return true;
}
