/**
 * DatabaseManager header file class
 *
 * @date 12-09-2026
 */

#pragma once
#include "utils/models/message.h"
#include "utils/models/user.h"
#include <initializer_list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

// import sqlite3
struct sqlite3;
struct sqlite3_stmt;

class DatabaseManager {
public:
  // define SqlRow and SqlResult
  using SqlRow = std::vector<std::string>;
  using SqlResult = std::vector<SqlRow>;

  // define ConstraintError
  class ConstraintError : public std::runtime_error {
  public:
    explicit ConstraintError(const std::string &what) : std::runtime_error(what) {}
  };

  // singleton instance getter
  static DatabaseManager &getInstance() {
    static DatabaseManager instance;
    return instance;
  }

  // lookup by username only (no password)
  std::optional<User> getUser(const std::string_view &username);

  // true if a row exists for this username
  bool userExists(const std::string_view &username);

  // create user — throws ConstraintError / runtime_error if username is taken
  User createUser(const std::string_view &username, const std::string_view &password,
                  const std::string_view &email);

  // authenticate: returns User or an error string
  std::variant<User, std::string> loginUser(const std::string_view &username,
                                            const std::string_view &password);

  // INSERT OR IGNORE — returns true if a new row was written
  bool saveMessage(const Message &message, std::string_view room);

  // messages for a room, newest first
  std::vector<Message> loadHistory(std::string_view room);

  // persist which node currently holds this user's socket in the room
  void setMembership(std::string_view username, std::string_view room, std::string_view nodeId);

  // drop a membership row
  void clearMembership(std::string_view username, std::string_view room);

  // true if username has a cluster presence row
  bool isUserOnline(std::string_view username);

  // upsert online_users (username → node holding the live socket)
  void setOnline(std::string_view username, std::string_view nodeId);

  // drop cluster presence for username
  void clearOnline(std::string_view username);

  // drop all room membership rows for username
  void clearAllMembership(std::string_view username);

  // delete copy constructor and assignment operator
  DatabaseManager(const DatabaseManager &) = delete;
  DatabaseManager &operator=(const DatabaseManager &) = delete;
  DatabaseManager(DatabaseManager &&) = delete;
  DatabaseManager &operator=(DatabaseManager &&) = delete;

  // destructor
  ~DatabaseManager();

private:
  std::mutex mutex;                    // the mutex for the database
  sqlite3 *db = nullptr;               // the database connection
  std::size_t DEFAULT_INTERVAL = 5000; // lock timeout interval

  // constructor
  DatabaseManager();

  // execute a SQL statement with no result rows (schema / PRAGMA)
  void exec(const char *sql);

  // prepare/bind under the db mutex. caller must hold mutex.
  sqlite3_stmt *prepareLocked(const char *sql, std::initializer_list<std::string_view> params);

  // SELECT — returns 0, 1, or many rows
  SqlResult query(const char *sql, std::initializer_list<std::string_view> params = {});

  // INSERT / UPDATE / DELETE — returns sqlite3_changes()
  int execute(const char *sql, std::initializer_list<std::string_view> params = {});

  static User userFromRow(const SqlRow &row);
};
