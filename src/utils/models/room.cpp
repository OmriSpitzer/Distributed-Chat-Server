/**
 * Room class
 *
 * @brief Room class to store a room and its metadata (name, type, privacy, users, history,
 * created_at)
 * @date 12-07-2026
 */

#include "utils/models/room.h"
#include "utils/models/message.h"
#include <atomic>
#include <ctime>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>

namespace {
std::atomic<uint64_t> next_room_id{0};
}

// constructor
Room::Room(std::string_view name, Room::RoomType type, Room::Privacy privacy)
    : id(std::to_string(++next_room_id)), name(name), type(type), privacy(privacy),
      created_at(std::time(nullptr)), num_users(0), num_messages(0) {}

// stream output operator
std::ostream &operator<<(std::ostream &out, Room &room) {
  out << "Room " << room.name << " (" << room.id << ", " << room.roomTypeToString() << "):\n";
  out << "created at: " << room.created_at << ", privacy: " << room.privacyToString() << "with "
      << room.num_users << " users and " << room.num_messages << " messages";
  return out;
}

// user operations
bool Room::addUser(User &user) {
  if (num_users >= ROOM_CAPACITY) {
    return false;
  }
  if (this->getUser(user.getEmail()) != nullptr) {
    return false;
  }
  users.insert({user.getEmail(), user});
  num_users++;
  return true;
}

bool Room::addMessage(User &from, User &to, std::string_view message) {
  try {
    Message msg(from, to, message);
    history.push_back(msg);
    num_messages++;
    return true;
  } catch (const std::exception &e) {
    return false;
  } catch (...) {
    return false;
  }
}

// authorization operations
bool Room::removeUser(std::string_view email, std::string_view reason,
                      std::string_view autherization) {

  // TODO: Implement authorization check

  auto user_ptr = this->getUser(email, autherization);
  if (user_ptr == nullptr) {
    return false;
  }
  users.erase(user_ptr->getEmail());
  num_users--;
  return true;
}
User *Room::getUser(std::string_view email, std::string_view authorization) {
  // TODO: Implement authorization check

  auto user_ptr = users.find(std::string(email));
  if (user_ptr == users.end()) {
    return nullptr;
  }
  return &user_ptr->second;
}
bool Room::isEmpty(std::string_view authorization) {
  // TODO: Implement authorization check
  return num_users == 0;
}

// health check
bool Room::ping() {

  // TODO: Implement ping check

  return !isEmpty();
}

// destructor
Room::~Room() {
  users.clear();
  history.clear();
}

// getters
std::string Room::roomTypeToString() {
  switch (type) {
  case RoomType::RESEARCH_AND_DEVELOPMENT:
    return "R&D";
  case RoomType::PRODUCTION:
    return "Production";
  case RoomType::QA:
    return "QA";
  case RoomType::DEVOPS:
    return "DevOps";
  case RoomType::SECURITY:
    return "Security";
  case RoomType::DESIGN:
    return "Design";
  case RoomType::MARKETING:
    return "Marketing";
  case RoomType::HR:
    return "HR";
  case RoomType::FINANCE:
    return "Finance";
  case RoomType::LEGAL:
    return "Legal";
  case RoomType::CUSTOMER_SUPPORT:
    return "Customer Support";
  default:
    return "Other";
  }
}
std::string Room::privacyToString() {
  switch (privacy) {
  case Privacy::PUBLIC:
    return "PUBLIC";
  case Privacy::PRIVATE:
    return "PRIVATE";
  default:
    return "OTHER";
  }
}