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
#include "utils/models/room.h"
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

// execute a SQL statement (supports multiple statements; same params bound to each)
int DatabaseManager::execute(const char *sql, std::initializer_list<std::string_view> params) {
  std::lock_guard<std::mutex> lock(mutex);

  const char *tail = sql;
  int changes = 0;
  while (tail && *tail) {
    while (*tail == ' ' || *tail == '\t' || *tail == '\n' || *tail == '\r') {
      ++tail;
    }
    if (*tail == '\0') {
      break;
    }

    sqlite3_stmt *raw = nullptr;
    const char *next = nullptr;
    if (sqlite3_prepare_v2(db, tail, -1, &raw, &next) != SQLITE_OK) {
      throw std::runtime_error(std::string("Failed to prepare SQL: ") + sqlite3_errmsg(db));
    }
    StmtPtr stmt(raw);

    int index = 1;
    for (const std::string_view param : params) {
      if (sqlite3_bind_text(stmt.get(), index, param.data(), static_cast<int>(param.size()),
                            SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to bind SQL parameter: ") +
                                 sqlite3_errmsg(db));
      }
      ++index;
    }

    const int rc = sqlite3_step(stmt.get());
    if (rc == SQLITE_CONSTRAINT) {
      throw ConstraintError(sqlite3_errmsg(db));
    }
    if (rc != SQLITE_DONE) {
      throw std::runtime_error(std::string("Failed to step SQL: ") + sqlite3_errmsg(db));
    }
    changes = sqlite3_changes(db);
    tail = next;
  }

  return changes;
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

// update username and/or password (empty password keeps the existing hash)
User DatabaseManager::updateUser(const std::string_view &currentUsername,
                                 const std::string_view &newUsername,
                                 const std::string_view &newPassword,
                                 const std::string_view &email) {
  auto existing = getUser(currentUsername);
  if (!existing) {
    throw std::runtime_error("User not found");
  }
  if (existing->getEmail() != email) {
    throw std::runtime_error("Email does not match profile");
  }

  const std::string current(currentUsername);
  const std::string desired(newUsername);
  if (desired.empty()) {
    throw std::runtime_error("Username is required");
  }

  if (desired != current && userExists(desired)) {
    throw ConstraintError("Username already taken");
  }

  if (!newPassword.empty()) {
    const std::string passwordHash = Authentication::hashPassword(newPassword);
    execute(db::sql::update_user_password, {passwordHash, current});
  }

  if (desired != current) {
    // membership / online_users FK username while renaming; briefly disable checks
    exec("PRAGMA foreign_keys=OFF;");
    try {
      execute(db::sql::update_user, {desired, current});
    } catch (...) {
      exec("PRAGMA foreign_keys=ON;");
      throw;
    }
    exec("PRAGMA foreign_keys=ON;");
  }

  auto updated = getUser(desired);
  if (!updated) {
    throw std::runtime_error("User missing after update");
  }
  return *updated;
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
bool DatabaseManager::saveMessage(const Message &message, int roomId) {
  const std::string createdAt = std::to_string(static_cast<long long>(message.getTimestamp()));
  const std::string username = message.getFrom().getUsername();
  const std::string email = message.getFrom().getEmail();
  const std::string roomIdStr = std::to_string(roomId);

  const int changes =
      execute(db::sql::save_message, {message.getId(), roomIdStr, username, email,
                                      message.getContent(), createdAt, config::NODE_ID});
  return changes > 0;
}

// load message history
std::vector<Message> DatabaseManager::loadHistory(int roomId) {
  const SqlResult rows = query(db::sql::load_history, {std::to_string(roomId)});
  std::vector<Message> messages;
  messages.reserve(rows.size());

  for (const SqlRow &row : rows) {
    messages.push_back(messageFromRow(row));
  }
  return messages;
}

// set user membership
void DatabaseManager::setMembership(std::string_view username, int roomId,
                                    std::string_view nodeId) {
  execute(db::sql::set_membership, {username, std::to_string(roomId), nodeId});
}

// clear user membership
void DatabaseManager::clearMembership(std::string_view username, int roomId) {
  execute(db::sql::clear_membership, {username, std::to_string(roomId)});
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

// convert a SQL row to a User object
User DatabaseManager::userFromRow(const SqlRow &row) {
  // check if the row has the required columns
  if (row.size() < 3) {
    throw std::runtime_error("users row is missing columns");
  }
  return User(row[0], row[1], User::stringToType(row[2]));
}

// convert a SQL row to a Room object
Room DatabaseManager::roomFromRow(const SqlRow &row) {
  // check if the row has the required columns
  if (row.size() < 4) {
    throw std::runtime_error("rooms row is missing columns");
  }

  int id = 0;
  try {
    id = std::stoi(row[0]);
  } catch (const std::exception &) {
    throw std::runtime_error("invalid rooms.id value");
  }
  return Room(id, row[1], Room::stringToRoomType(row[2]), Room::stringToPrivacy(row[3]));
}

// convert a SQL row to a Message object
Message DatabaseManager::messageFromRow(const SqlRow &row) {
  if (row.size() < 6) {
    throw std::runtime_error("messages row is missing columns");
  }

  // create the created at timestamp
  std::time_t createdAt = 0;
  try {
    createdAt = static_cast<std::time_t>(std::stoll(row[3]));
  } catch (const std::exception &) {
    throw std::runtime_error("invalid messages.created_at value");
  }

  // create the users
  const User from(row[1], row[4], User::stringToType(row[5]));
  const User to("", "", User::UserType::GUEST);
  return Message(from, to, row[2], row[0], createdAt);
}

// create a new room — id comes from SQLite AUTOINCREMENT (last_insert_rowid)
Room DatabaseManager::createRoom(std::string_view name, Room::RoomType type, Room::Privacy privacy,
                                 std::string_view creatorEmail) {
  const std::string roomType = Room::roomTypeToString(type);
  const std::string privacyType = Room::privacyToString(privacy);

  Room created(0, name, type, privacy);
  {
    std::lock_guard<std::mutex> lock(mutex);
    try {
      StmtPtr stmt(prepareLocked(db::sql::create_room, {name, roomType, privacyType}));
      const int rc = sqlite3_step(stmt.get());
      if (rc == SQLITE_CONSTRAINT) {
        throw ConstraintError("Room already exists");
      }
      if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("Failed to create room: ") + sqlite3_errmsg(db));
      }
      const int id = static_cast<int>(sqlite3_last_insert_rowid(db));
      created = Room(id, name, type, privacy);
    } catch (const ConstraintError &) {
      throw;
    }
  }

  if (!creatorEmail.empty()) {
    addToAllowList(created.getId(), creatorEmail, true);
  }
  return created;
}

// get a room by id
Room DatabaseManager::getRoom(int id) {
  const SqlResult rows = query(db::sql::get_room, {std::to_string(id)});
  if (rows.empty()) {
    throw std::runtime_error("Room not found");
  }
  return roomFromRow(rows.front());
}

// list all rooms
std::vector<Room> DatabaseManager::listRooms() {
  const SqlResult rows = query(db::sql::list_rooms);
  std::vector<Room> rooms;
  rooms.reserve(rows.size());
  for (const SqlRow &row : rows) {
    rooms.push_back(roomFromRow(row));
  }
  return rooms;
}

// delete a room
void DatabaseManager::deleteRoom(int id) {
  if (id < 3) {
    throw std::runtime_error("Cannot delete default rooms");
  }
  const int changes = execute(db::sql::delete_room, {std::to_string(id)});
  if (changes == 0) {
    throw ConstraintError("Room not found");
  }
}

// get the allow list for a room
std::vector<std::string> DatabaseManager::getAllowList(int roomId) {
  const SqlResult rows = query(db::sql::get_allow_list, {std::to_string(roomId)});
  std::vector<std::string> allowList;
  allowList.reserve(rows.size());
  for (const SqlRow &row : rows) {
    if (!row.empty()) {
      allowList.push_back(row[0]);
    }
  }
  return allowList;
}

// true if email is on the room allow list
bool DatabaseManager::isAllowed(int roomId, std::string_view email) {
  return !query(db::sql::is_allowed, {std::to_string(roomId), email}).empty();
}

// true if email is the allow-list creator for the room
bool DatabaseManager::isAllowListCreator(int roomId, std::string_view email) {
  return !query(db::sql::is_allow_list_creator, {std::to_string(roomId), email}).empty();
}

// add a user to the allow list for a room
void DatabaseManager::addToAllowList(int roomId, std::string_view email, bool creator) {
  execute(db::sql::add_user_allow_list, {std::to_string(roomId), email, creator ? "1" : "0"});
}

// remove a user from the allow list for a room
void DatabaseManager::removeFromAllowList(int roomId, std::string_view email) {
  execute(db::sql::remove_user_allow_list, {std::to_string(roomId), email});
}