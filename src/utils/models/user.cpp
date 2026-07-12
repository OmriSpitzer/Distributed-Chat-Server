/**
 * User class
 *
 * @brief User class to store a user and its metadata (username, email, user_type)
 * @date 12-07-2026
 */

#include "utils/models/user.h"

// constructor
User::User(std::string_view username, std::string_view email, User::UserType user_type)
    : _username(username), _email(email), _user_type(user_type) {}

// stream output operator
std::ostream &operator<<(std::ostream &out, const User &user) {
  out << "User: " << user.getUsername() << " (" << user.getEmail() << ") - "
      << User::typeToString(user.getUserType());
  return out;
}

// equality operator
bool User::operator==(const User &other) const { return _email == other._email; }
bool User::operator!=(const User &other) const { return _email != other._email; }

// getters
std::string User::getUsername() const { return _username; }
std::string User::getEmail() const { return _email; }
User::UserType User::getUserType() const { return _user_type; }

std::string User::typeToString(User::UserType user_type) {
  switch (user_type) {
  case User::UserType::ADMIN:
    return "ADMIN";
  case User::UserType::USER:
    return "USER";
  case User::UserType::GUEST:
    return "GUEST";
  }
  return "UNKNOWN";
}

// setters
void User::setUsername(std::string_view username) { _username = username; }
void User::setEmail(std::string_view email) { _email = email; }
void User::setUserType(User::UserType user_type) { _user_type = user_type; }