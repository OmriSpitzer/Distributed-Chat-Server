/**
 * DatabaseManager class
 *
 * @brief Simple pipe-delimited text database backing the chat server.
 * @date 14-07-2026
 */

#include "server/database_manager.h"
#include "config/config.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

constexpr const char *USERS_HEADER = "[USERS]";
constexpr const char *ROOMS_HEADER = "[ROOMS]";
constexpr const char *MESSAGES_HEADER = "[MESSAGES]";

// map between UserType and its integer representation stored on disk
int userTypeToInt(User::UserType type) { return static_cast<int>(type); }

User::UserType intToUserType(int value) {
  switch (value) {
  case 0:
    return User::UserType::ADMIN;
  case 1:
    return User::UserType::USER;
  default:
    return User::UserType::GUEST;
  }
}

// split a line on '|'
std::vector<std::string> split(const std::string &line) {
  std::vector<std::string> parts;
  std::stringstream stream(line);
  std::string token;
  while (std::getline(stream, token, '|')) {
    parts.push_back(token);
  }
  return parts;
}

// in-memory view of the three database sections
struct Sections {
  std::vector<std::string> users;
  std::vector<std::string> rooms;
  std::vector<std::string> messages;
};

} // namespace

// singleton constructor design pattern
DatabaseManager::DatabaseManager() : dbPath(config::DB_PATH) { ensureFile(); }

void DatabaseManager::ensureFile() {
  std::filesystem::path path(dbPath);
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path());
  }
  if (std::filesystem::exists(path)) {
    return;
  }
  std::ofstream out(dbPath);
  out << USERS_HEADER << '\n' << ROOMS_HEADER << '\n' << MESSAGES_HEADER << '\n';
}

namespace {

// read the database file into its three sections
Sections readSections(const std::string &dbPath) {
  Sections sections;
  std::ifstream in(dbPath);
  std::string line;
  std::string current;
  while (std::getline(in, line)) {
    if (line == USERS_HEADER) {
      current = USERS_HEADER;
    } else if (line == ROOMS_HEADER) {
      current = ROOMS_HEADER;
    } else if (line == MESSAGES_HEADER) {
      current = MESSAGES_HEADER;
    } else if (line.empty()) {
      continue;
    } else if (current == USERS_HEADER) {
      sections.users.push_back(line);
    } else if (current == ROOMS_HEADER) {
      sections.rooms.push_back(line);
    } else if (current == MESSAGES_HEADER) {
      sections.messages.push_back(line);
    }
  }
  return sections;
}

// write the three sections back to the database file
void writeSections(const std::string &dbPath, const Sections &sections) {
  std::ofstream out(dbPath, std::ios::trunc);
  out << USERS_HEADER << '\n';
  for (const auto &row : sections.users) {
    out << row << '\n';
  }
  out << ROOMS_HEADER << '\n';
  for (const auto &row : sections.rooms) {
    out << row << '\n';
  }
  out << MESSAGES_HEADER << '\n';
  for (const auto &row : sections.messages) {
    out << row << '\n';
  }
}

} // namespace

bool DatabaseManager::createUser(const User &user, const std::string &password) {
  if (userExists(user.getUsername())) {
    return false;
  }
  Sections sections = readSections(dbPath);
  std::ostringstream row;
  row << user.getUsername() << '|' << user.getEmail() << '|' << userTypeToInt(user.getUserType())
      << '|' << password;
  sections.users.push_back(row.str());
  writeSections(dbPath, sections);
  return true;
}

bool DatabaseManager::userExists(const std::string &username) {
  Sections sections = readSections(dbPath);
  for (const auto &row : sections.users) {
    auto parts = split(row);
    if (!parts.empty() && parts[0] == username) {
      return true;
    }
  }
  return false;
}

User DatabaseManager::findUser(const std::string &username) {
  Sections sections = readSections(dbPath);
  for (const auto &row : sections.users) {
    auto parts = split(row);
    if (parts.size() >= 3 && parts[0] == username) {
      return User(parts[0], parts[1], intToUserType(std::stoi(parts[2])));
    }
  }
  throw std::runtime_error("User not found: " + username);
}

std::string DatabaseManager::getPassword(const std::string &username) {
  Sections sections = readSections(dbPath);
  for (const auto &row : sections.users) {
    auto parts = split(row);
    if (parts.size() >= 4 && parts[0] == username) {
      return parts[3];
    }
  }
  return "";
}

bool DatabaseManager::saveMessage(const Message &message, const std::string &roomId) {
  Sections sections = readSections(dbPath);
  std::ostringstream row;
  row << message.getId() << '|' << roomId << '|' << message.getFrom().getUsername() << '|'
      << message.getTo().getUsername() << '|' << message.getContent() << '|'
      << message.getTimestamp();
  sections.messages.push_back(row.str());
  writeSections(dbPath, sections);
  return true;
}

std::vector<Message> DatabaseManager::loadMessages(const std::string &roomId) {
  Sections sections = readSections(dbPath);
  std::vector<Message> result;
  for (const auto &row : sections.messages) {
    auto parts = split(row);
    if (parts.size() < 6) {
      continue;
    }
    if (!roomId.empty() && parts[1] != roomId) {
      continue;
    }
    // keep reconstructed users alive so Message references stay valid
    userPool.push_back(std::make_unique<User>(parts[2], "", User::UserType::USER));
    userPool.push_back(std::make_unique<User>(parts[3], "", User::UserType::USER));
    User &from = *userPool[userPool.size() - 2];
    User &to = *userPool[userPool.size() - 1];
    result.emplace_back(from, to, parts[4]);
  }
  return result;
}

bool DatabaseManager::createRoom(const Room &room) {
  Sections sections = readSections(dbPath);
  for (const auto &existing : sections.rooms) {
    auto parts = split(existing);
    if (!parts.empty() && parts[0] == room.getId()) {
      return false;
    }
  }
  std::ostringstream row;
  row << room.getId() << '|' << room.getName() << '|' << static_cast<int>(room.getType()) << '|'
      << static_cast<int>(room.getPrivacy());
  sections.rooms.push_back(row.str());
  writeSections(dbPath, sections);
  return true;
}
