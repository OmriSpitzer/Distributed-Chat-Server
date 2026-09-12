/**
 * DatabaseManager class
 *
 * @brief Database manager for this chat server node.
 * @date 12-09-2026
 */

#include "server/database_manager.h"
#include "auth/authentication.h"
#include "config/config.h"
#include "sql_schemas.h"
#include "sqlite3.h"
#include "utils/models/message.h"
#include "utils/models/user.h"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
// finalize statement
struct FinalizeStmt {
  void operator()(sqlite3_stmt *stmt) const noexcept {
    if (stmt) {
      sqlite3_finalize(stmt);
    }
  }
};

// statement pointer
using StmtPtr = std::unique_ptr<sqlite3_stmt, FinalizeStmt>;
} // namespace

// constructor
DatabaseManager::DatabaseManager() {
  // create database directory if it doesn't exist
  const std::filesystem::path dbPath(config::DB_PATH);
  if (dbPath.has_parent_path()) {
    std::filesystem::create_directories(dbPath.parent_path());
  }

  // open sqlite database file
  if (sqlite3_open(config::DB_PATH.c_str(), &db) != SQLITE_OK) {
    std::string message = db ? sqlite3_errmsg(db) : "unknown error";
    if (db) {
      sqlite3_close(db);
      db = nullptr;
    }
    throw std::runtime_error("Failed to open database: " + message);
  }

  // set database pragma
  exec("PRAGMA journal_mode=WAL;");
  exec("PRAGMA foreign_keys=ON;");
  sqlite3_busy_timeout(db, static_cast<int>(DEFAULT_INTERVAL));

  // initialize database tables
  exec(db::sql::init);
}

// destructor
DatabaseManager::~DatabaseManager() {
  if (db) {
    sqlite3_close(db);
    db = nullptr;
  }
}

// execute a SQL statement
void DatabaseManager::exec(const char *sql) {
  std::lock_guard<std::mutex> lock(mutex);

  char *err = nullptr;
  if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
    std::string errorMessage = err ? err : "unknown error";
    sqlite3_free(err);
    throw std::runtime_error("Failed to execute SQL statement: " + errorMessage);
  }
}

// prepare a SQL statement
sqlite3_stmt *DatabaseManager::prepareLocked(const char *sql,
                                             std::initializer_list<std::string_view> params) {
  // prepare the statement
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(std::string("Failed to prepare SQL: ") + sqlite3_errmsg(db));
  }

  // bind parameters
  int index = 1;
  for (const std::string_view param : params) {
    if (sqlite3_bind_text(stmt, index, param.data(), static_cast<int>(param.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
      const std::string message = sqlite3_errmsg(db);
      sqlite3_finalize(stmt);
      throw std::runtime_error("Failed to bind SQL parameter: " + message);
    }
    ++index;
  }
  return stmt;
}

// execute a query
DatabaseManager::SqlResult DatabaseManager::query(const char *sql,
                                                  std::initializer_list<std::string_view> params) {
  std::lock_guard<std::mutex> lock(mutex);
  StmtPtr stmt(prepareLocked(sql, params));

  // execute the statement
  SqlResult rows;
  while (true) {
    const int rc = sqlite3_step(stmt.get());

    // if the statement returns a row, add it to the result
    if (rc == SQLITE_ROW) {
      SqlRow row;
      const int columns = sqlite3_column_count(stmt.get());
      row.reserve(static_cast<std::size_t>(columns));
      for (int col = 0; col < columns; ++col) {
        const unsigned char *text = sqlite3_column_text(stmt.get(), col);
        row.emplace_back(text ? reinterpret_cast<const char *>(text) : "");
      }
      rows.push_back(std::move(row));
    }

    // if the statement returns done, break the loop
    else if (rc == SQLITE_DONE) {
      break;
    }

    // error handling
    else {
      throw std::runtime_error(std::string("Failed to step SQL: ") + sqlite3_errmsg(db));
    }
  }

  return rows;
}

// execute a SQL statement
int DatabaseManager::execute(const char *sql, std::initializer_list<std::string_view> params) {
  std::lock_guard<std::mutex> lock(mutex);

  // prepare the statement and lock the database
  StmtPtr stmt(prepareLocked(sql, params));

  // execute the statement and get the result
  const int rc = sqlite3_step(stmt.get());
  if (rc == SQLITE_CONSTRAINT) {
    throw ConstraintError(sqlite3_errmsg(db));
  }
  if (rc != SQLITE_DONE) {
    throw std::runtime_error(std::string("Failed to step SQL: ") + sqlite3_errmsg(db));
  }

  // return the number of changes
  return sqlite3_changes(db);
}

// convert a SQL row to a User object
User DatabaseManager::userFromRow(const SqlRow &row) {
  // check if the row has the required columns
  if (row.size() < 3) {
    throw std::runtime_error("users row is missing columns");
  }
  return User(row[0], row[1], User::stringToType(row[2]));
}

// get user by username only
std::optional<User> DatabaseManager::getUser(const std::string_view &username) {
  const SqlResult rows = query(db::sql::get_user, {username});
  if (rows.empty()) {
    return std::nullopt;
  }
  return userFromRow(rows.front());
}

// check if a user exists
bool DatabaseManager::userExists(const std::string_view &username) {
  return !query(db::sql::user_exists, {username}).empty();
}

// create user
User DatabaseManager::createUser(const std::string_view &username, const std::string_view &password,
                                 const std::string_view &email) {
  const std::string passwordHash = Authentication::hashPassword(password);
  const std::string userType = User::typeToString(User::UserType::USER);

  try {
    execute(db::sql::create_user, {username, email, passwordHash, userType});
  } catch (const ConstraintError &) {
    throw ConstraintError("User already exists");
  }
  return User(username, email, User::UserType::USER);
}

// login user
std::variant<User, std::string> DatabaseManager::loginUser(const std::string_view &username,
                                                           const std::string_view &password) {
  static const std::string kAuthFailed = "Invalid username or password";
  const SqlResult rows = query(db::sql::login_user, {username});

  // if the user does not exist or the password is incorrect, return an error
  if (rows.empty() || rows.front().size() < 4) {
    return kAuthFailed;
  }

  // if the password is incorrect, return an error
  if (!Authentication::checkPassword(password, rows.front()[3])) {
    return kAuthFailed;
  }
  return userFromRow(rows.front());
}

// save a message
bool DatabaseManager::saveMessage(const Message &message, std::string_view room) {
  const std::string createdAt = std::to_string(static_cast<long long>(message.getTimestamp()));
  const std::string username = message.getFrom().getUsername();

  const int changes =
      execute(db::sql::save_message,
              {message.getId(), room, username, message.getContent(), createdAt, config::NODE_ID});
  return changes > 0;
}

// load message history
std::vector<Message> DatabaseManager::loadHistory(std::string_view roomId) {
  const SqlResult rows = query(db::sql::load_history, {roomId});
  std::vector<Message> messages;
  messages.reserve(rows.size());

  const User to("", "", User::UserType::GUEST);
  for (const SqlRow &row : rows) {
    if (row.size() < 6) {
      throw std::runtime_error("messages row is missing columns");
    }
    std::time_t createdAt = 0;
    try {
      createdAt = static_cast<std::time_t>(std::stoll(row[3]));
    } catch (const std::exception &) {
      throw std::runtime_error("invalid messages.created_at value");
    }
    const User from(row[1], row[4], User::stringToType(row[5]));
    messages.emplace_back(from, to, row[2], row[0], createdAt);
  }
  return messages;
}

// set user membership
void DatabaseManager::setMembership(std::string_view username, std::string_view room,
                                    std::string_view nodeId) {
  execute(db::sql::set_membership, {username, room, nodeId});
}

// clear user membership
void DatabaseManager::clearMembership(std::string_view username, std::string_view room) {
  execute(db::sql::clear_membership, {username, room});
}

// check if a user is online
bool DatabaseManager::isUserOnline(std::string_view username) {
  return !query(db::sql::is_user_online, {username}).empty();
}

// set user online
void DatabaseManager::setOnline(std::string_view username, std::string_view nodeId) {
  execute(db::sql::set_online, {username, nodeId});
}

// clear user online
void DatabaseManager::clearOnline(std::string_view username) {
  execute(db::sql::clear_online, {username});
}

// clear all user membership
void DatabaseManager::clearAllMembership(std::string_view username) {
  execute(db::sql::clear_all_membership, {username});
}