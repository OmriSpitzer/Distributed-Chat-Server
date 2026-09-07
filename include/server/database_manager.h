/**
 * DatabaseManager header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include "data/data.h"
#include "utils/models/user.h"
#include <any>
#include <string>
#include <string_view>

class DatabaseManager {
public:
  // singleton instance getter
  static DatabaseManager &getInstance() {
    static DatabaseManager instance;
    return instance;
  }

  // get user
  std::any getUser(const std::string_view &username, const std::string_view &password);

  // create user
  User createUser(const std::string_view &username, const std::string_view &password,
                  const std::string_view &email);

  // delete copy constructor and assignment operator
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

private:
  // constructor
  DatabaseManager();

  Data data = Data(); // the data of the database
};
