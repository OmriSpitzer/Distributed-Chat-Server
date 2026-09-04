/**
 * Authentication class
 *
 * @brief Basic username/password authentication backed by the text database.
 * @date 03-09-2026
 */

#include "auth/authentication.h"
#include "server/database_manager.h"
#include "utils/models/logger.h"
#include <iostream>
#include <stdexcept>

// register a user
User Authentication::registerUser(const std::string &username, const std::string &password) {
  // create a user
  User user(username, username + "@chat.local", User::UserType::USER);

  // #TODO: create the user in the database

  Logger::logInfo("Authentication", "User created: " + username);
  return user;
}

// login a user
User Authentication::login(const std::string &username, const std::string &password) {
  // verify the password
  if (!verifyPassword(username, password)) {
    throw std::runtime_error("Invalid credentials for: " + username);
  }

  // find the user and return it
  DatabaseManager &db = DatabaseManager::getInstance();
  User user = db.findUser(username);

  // #TODO: verify the user is not already logged in or exists

  Logger::logInfo("Authentication", "User logged in: " + username);
  return user;
}

// logout a user
void Authentication::logout(const std::string &username) {
  // #TODO: logout the user from the database

  Logger::logInfo("Authentication", "User logged out: " + username);
}

bool Authentication::verifyPassword(const std::string &username, const std::string &password) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: verify the password is correct

  return true;
}
