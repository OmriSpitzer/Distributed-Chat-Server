/**
 * DatabaseManager header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include "data/data.h"
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

  // delete copy constructor and assignment operator
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

private:
  // constructor
  DatabaseManager();

  Data data = Data(); // the data of the database
};
