/**
 * UserManager header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/user.h"
#include <string>
#include <unordered_map>

class UserManager {
public:
  // add a user
  bool addUser(const User &user);

  // remove a user
  bool removeUser(const User &user);

  // find a user
  User findUser(const std::string &username);

  // set a user's status
  bool setStatus(const User &user, const std::string &status);

private:
  // online users keyed by username
  std::unordered_map<std::string, User> users;
};
