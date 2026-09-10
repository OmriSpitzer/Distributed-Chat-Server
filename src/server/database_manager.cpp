/**
 * DatabaseManager class
 *
 * @brief SQLite-backed database for this chat server node.
 * @date 08-09-2026
 */

#include "server/database_manager.h"
#include "config/config.h"
#include "sql_schemas.h"
#include "sqlite3.h"
#include "utils/models/user.h"
#include <any>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace fs = std::filesystem;

namespace {
struct FinalizeStmt {
  void operator()(sqlite3_stmt *stmt) const noexcept {
    if (stmt) {
      sqlite3_finalize(stmt);
    }
  }
};
using StmtPtr = std::unique_ptr<sqlite3_stmt, FinalizeStmt>;
} // namespace

// singleton constructor design pattern
DatabaseManager::DatabaseManager() {
  const fs::path dbPath(config::DB_PATH);
  if (dbPath.has_parent_path()) {
    fs::create_directories(dbPath.parent_path());
  }

  if (sqlite3_open(config::DB_PATH.c_str(), &db) != SQLITE_OK) {
    std::string message = db ? sqlite3_errmsg(db) : "unknown error";
    if (db) {
      sqlite3_close(db);
      db = nullptr;
    }
    throw std::runtime_error("Failed to open database: " + message);
  }

  exec("PRAGMA journal_mode=WAL;");
  exec("PRAGMA foreign_keys=ON;");
  exec("PRAGMA busy_timeout=5000;");
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

sqlite3_stmt *DatabaseManager::prepareLocked(const char *sql,
                                             std::initializer_list<std::string_view> params) {
  sqlite3_stmt *stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(std::string("Failed to prepare SQL: ") + sqlite3_errmsg(db));
  }

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

  SqlResult rows;
  while (true) {
    const int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_ROW) {
      SqlRow row;
      const int columns = sqlite3_column_count(stmt.get());
      row.reserve(static_cast<std::size_t>(columns));
      for (int col = 0; col < columns; ++col) {
        const unsigned char *text = sqlite3_column_text(stmt.get(), col);
        row.emplace_back(text ? reinterpret_cast<const char *>(text) : "");
      }
      rows.push_back(std::move(row));
    } else if (rc == SQLITE_DONE) {
      break;
    } else {
      throw std::runtime_error(std::string("Failed to step SQL: ") + sqlite3_errmsg(db));
    }
  }

  return rows;
}

int DatabaseManager::execute(const char *sql, std::initializer_list<std::string_view> params) {
  std::lock_guard<std::mutex> lock(mutex);
  StmtPtr stmt(prepareLocked(sql, params));

  const int rc = sqlite3_step(stmt.get());
  if (rc == SQLITE_CONSTRAINT) {
    throw ConstraintError(sqlite3_errmsg(db));
  }
  if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
    throw std::runtime_error(std::string("Failed to step SQL: ") + sqlite3_errmsg(db));
  }
  return sqlite3_changes(db);
}

User DatabaseManager::userFromRow(const SqlRow &row) {
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

bool DatabaseManager::userExists(const std::string_view &username) {
  return !query(db::sql::user_exists, {username}).empty();
}

// create user
User DatabaseManager::createUser(const std::string_view &username, const std::string_view &password,
                                 const std::string_view &email) {
  try {
    execute(db::sql::create_user, {username, email, password, "USER"});
  } catch (const ConstraintError &) {
    throw std::runtime_error("User already exists");
  }

  return User(username, email, User::UserType::USER);
}

// login user
std::any DatabaseManager::loginUser(const std::string_view &username,
                                    const std::string_view &password) {
  const SqlResult rows = query(db::sql::login_user, {username, password});
  if (!rows.empty()) {
    return userFromRow(rows.front());
  }
  if (!userExists(username)) {
    return std::string("User not found");
  }
  return std::string("Password is incorrect");
}

bool DatabaseManager::saveMessage(const Message &message, std::string_view room) {
  const std::string createdAt = std::to_string(static_cast<long long>(message.getTimestamp()));
  const std::string username = message.getFrom().getUsername();

  const int changes =
      execute(db::sql::save_message,
              {message.getId(), room, username, message.getContent(), createdAt, config::NODE_ID});
  return changes > 0;
}

std::vector<Message> DatabaseManager::loadHistory(std::string_view roomId) {
  const SqlResult rows = query(db::sql::load_history, {roomId});
  std::vector<Message> messages;
  messages.reserve(rows.size());

  // room history has no recipient column; Message still requires a `to` user
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

void DatabaseManager::setMembership(std::string_view username, std::string_view room,
                                    std::string_view nodeId) {
  execute(db::sql::set_membership, {username, room, nodeId});
}

void DatabaseManager::clearMembership(std::string_view username, std::string_view room) {
  execute(db::sql::clear_membership, {username, room});
}
