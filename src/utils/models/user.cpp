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
#include <stdexcept>

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
  default:
    return "GUEST";
  }
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

// string to type
User::UserType User::stringToType(const std::string_view &type) {
  if (type == "ADMIN") {
    return User::UserType::ADMIN;
  }
  if (type == "USER") {
    return User::UserType::USER;
  }
  return User::UserType::GUEST;
}

// serialize
std::string User::serialize() const {
  return "user(" + this->username + "|" + this->email + "|" + User::typeToString(this->user_type) +
         ")";
}

// deserialize
User User::deserialize(const std::string &serialized) {
  static const std::string prefix = "user(";
  if (serialized.size() < prefix.size() + 1 || serialized.compare(0, prefix.size(), prefix) != 0 ||
      serialized.back() != ')') {
    throw std::invalid_argument("Invalid serialized user");
  }

  const std::string body = serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);
  const std::size_t first = body.find('|');
  const std::size_t second = (first == std::string::npos) ? std::string::npos : body.find('|', first + 1);
  if (first == std::string::npos || second == std::string::npos) {
    throw std::invalid_argument("Invalid serialized user");
  }

  const std::string username = body.substr(0, first);
  const std::string email = body.substr(first + 1, second - first - 1);
  const std::string userType = body.substr(second + 1);
  return User(username, email, User::stringToType(userType));
}