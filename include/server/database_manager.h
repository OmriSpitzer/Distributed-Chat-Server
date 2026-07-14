/**
 * DatabaseManager header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/message.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <memory>
#include <string>
#include <vector>

class DatabaseManager {
public:
  // singleton instance getter
  static DatabaseManager &getInstance() {
    static DatabaseManager instance;
    return instance;
  }

  // create a user (also stores the password)
  bool createUser(const User &user, const std::string &password = "");

  // find a user by username; throws std::runtime_error if not found
  User findUser(const std::string &username);

  // check whether a user exists
  bool userExists(const std::string &username);

  // return the stored password for a user (empty if none/not found)
  std::string getPassword(const std::string &username);

  // save a message
  bool saveMessage(const Message &message, const std::string &roomId = "");

  // load messages for a room
  std::vector<Message> loadMessages(const std::string &roomId);

  // create a room
  bool createRoom(const Room &room);

  // delete copy constructor and assignment operator
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;

private:
  std::string dbPath; // path to the text database file

  // keeps loaded users alive so Message references stay valid
  std::vector<std::unique_ptr<User>> userPool;

  // constructor
  DatabaseManager();

  // ensures the database file (and its directory) exists
  void ensureFile();
};
