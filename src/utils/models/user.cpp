/**
 * User class
 *
 * @brief User class to store a user and its metadata (username, email, user_type)
 * @date 12-07-2026
 */

#include "utils/models/user.h"
#include <iomanip>
#include <random>
#include <sstream>

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

// generate a random username
User User::anonymousUser() {
  // create a random number generator
  static std::mt19937 rng{std::random_device{}()};
  static std::uniform_int_distribution<int> dist(0, 9999);

  // create a string stream
  std::ostringstream id;

  // pad the number with 0s and convert to string
  id << std::setw(7) << std::setfill('0') << dist(rng);
  const std::string suffix = id.str();
  const std::string genUsername = "anon" + suffix;

  return User(genUsername, genUsername + "@local", User::UserType::GUEST);
}