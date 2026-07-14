/**
 * Authentication class
 *
 * @brief Basic username/password authentication backed by the text database.
 * @date 14-07-2026
 */

#include "auth/authentication.h"
#include "server/database_manager.h"
#include <iostream>
#include <stdexcept>

User Authentication::registerUser(const std::string &username, const std::string &password) {
  User user(username, username + "@chat.local", User::UserType::USER);
  if (!DatabaseManager::getInstance().createUser(user, password)) {
    throw std::runtime_error("User already exists: " + username);
  }
  return user;
}

User Authentication::login(const std::string &username, const std::string &password) {
  if (!verifyPassword(username, password)) {
    throw std::runtime_error("Invalid credentials for: " + username);
  }
  return DatabaseManager::getInstance().findUser(username);
}

void Authentication::logout(const std::string &username) {
  std::cout << "User logged out: " << username << '\n';
}

bool Authentication::verifyPassword(const std::string &username, const std::string &password) {
  auto &db = DatabaseManager::getInstance();
  if (!db.userExists(username)) {
    return false;
  }
  return db.getPassword(username) == password;
}
