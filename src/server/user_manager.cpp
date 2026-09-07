/**
 * UserManager class
 *
 * @brief Tracks online users in memory.
 * @date 07-09-2026
 */

#include "server/user_manager.h"
#include <stdexcept>

bool UserManager::addUser(const User &user) {
  if (users.find(user.getUsername()) != users.end()) {
    return false;
  }
  users.insert({user.getUsername(), user});
  return true;
}

bool UserManager::removeUser(const User &user) { return users.erase(user.getUsername()) > 0; }

User UserManager::findUser(const std::string &username) {
  auto it = users.find(username);
  if (it != users.end()) {
    return it->second;
  }
  throw std::runtime_error("User not found: " + username);
}

bool UserManager::setStatus(const User &user, const std::string &status) {
  (void)status;
  return users.find(user.getUsername()) != users.end();
}
