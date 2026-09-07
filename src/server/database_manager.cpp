/**
 * DatabaseManager class
 *
 * @brief In-memory stub database backing the chat server.
 * @date 07-09-2026
 */

#include "server/database_manager.h"
#include "utils/models/user.h"
#include <any>
#include <string>

// singleton constructor design pattern
DatabaseManager::DatabaseManager() {}

// get user
std::any DatabaseManager::getUser(const std::string_view &username,
                                  const std::string_view &password) {
  for (const auto &row : Data::users) {
    if (row.at("username") != username) {
      continue;
    }
    if (row.at("password") != password) {
      return std::string("Invalid password");
    }
    return User(row.at("username"), row.at("email"), User::stringToType(row.at("type")));
  }
  return std::string("User not found");
}
