/**
 * User class
 *
 * @brief User class to store a user and its metadata (username, email, user_type)
 * @date 12-07-2026
 */

#include "utils/models/user.h"

// constructor
User::User(std::string_view username, std::string_view email, User::UserType user_type)
    : username(username), email(email), user_type(user_type) {}

// stream output operator
std::ostream &operator<<(std::ostream &out, const User &user) {
  out << "User: " << user.getUsername() << " (" << user.getEmail() << ") - "
      << User::typeToString(user.getUserType());
  return out;
}

// equality operator
bool User::operator==(const User &other) const { return this->email == other.email; }
bool User::operator!=(const User &other) const { return this->email != other.email; }

// getters
std::string User::getUsername() const { return this->username; }
std::string User::getEmail() const { return this->email; }
User::UserType User::getUserType() const { return this->user_type; }

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
void User::setUsername(std::string_view username) { this->username = username; }
void User::setEmail(std::string_view email) { this->email = email; }
void User::setUserType(User::UserType user_type) { this->user_type = user_type; }