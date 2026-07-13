/**
 * User header file class
 *
 * @brief User class to store a user and its metadata (username, email, user_type)
 * @date 12-07-2026
 */

#pragma once

#include <ostream>
#include <string>
#include <string_view>

class User {
public:
  // user type enum class
  enum class UserType { ADMIN, USER, GUEST };

  // constructor
  User(std::string_view username, std::string_view email, User::UserType user_type);

  // getters
  std::string getUsername() const;
  std::string getEmail() const;
  User::UserType getUserType() const;

  // setters
  void setUsername(std::string_view username);
  void setEmail(std::string_view email);
  void setUserType(User::UserType user_type);

  // stream output operator
  friend std::ostream &operator<<(std::ostream &out, const User &user);

  // equality operator
  bool operator==(const User &other) const;
  bool operator!=(const User &other) const;

private:
  std::string username;     // username
  std::string email;        // email
  User::UserType user_type; // user type
  static std::string typeToString(User::UserType user_type);
};