/**
 * UserManager class
 *
 * @brief Tracks online users in memory and persists them to the database.
 * @date 14-07-2026
 */

#include "server/user_manager.h"
#include "server/database_manager.h"
#include <iostream>

bool UserManager::addUser(const User &user) {
  if (users.find(user.getUsername()) != users.end()) {
    return false;
  }
  users.insert({user.getUsername(), user});
  DatabaseManager::getInstance().createUser(user);
  return true;
}

bool UserManager::removeUser(const User &user) { return users.erase(user.getUsername()) > 0; }

User UserManager::findUser(const std::string &username) {
  auto it = users.find(username);
  if (it != users.end()) {
    return it->second;
  }
  return DatabaseManager::getInstance().findUser(username);
}

bool UserManager::setStatus(const User &user, const std::string &status) {
  std::cout << "Status for " << user.getUsername() << ": " << status << '\n';
  return true;
}
