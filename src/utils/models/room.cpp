/**
 * Room class
 *
 * @brief Room class to store a room and its metadata.
 * @date 11-09-2026
 */
#include "utils/models/room.h"
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// type to string map
static const std::unordered_map<Room::RoomType, std::string> typeMap{
    {Room::RoomType::RESEARCH_AND_DEVELOPMENT, "R&D"},
    {Room::RoomType::PRODUCTION, "Production"},
    {Room::RoomType::QA, "QA"},
    {Room::RoomType::DEVOPS, "DevOps"},
    {Room::RoomType::SECURITY, "Security"},
    {Room::RoomType::DESIGN, "Design"},
    {Room::RoomType::MARKETING, "Marketing"},
    {Room::RoomType::HR, "HR"},
    {Room::RoomType::FINANCE, "Finance"},
    {Room::RoomType::LEGAL, "Legal"},
    {Room::RoomType::CUSTOMER_SUPPORT, "Customer Support"},
    {Room::RoomType::OTHER, "Other"},
    {Room::RoomType::LOBBY, "Lobby"},
};

// string to type map
static const std::unordered_map<std::string, Room::RoomType> stringToTypeMap{
    {"R&D", Room::RoomType::RESEARCH_AND_DEVELOPMENT},
    {"Production", Room::RoomType::PRODUCTION},
    {"QA", Room::RoomType::QA},
    {"DevOps", Room::RoomType::DEVOPS},
    {"Security", Room::RoomType::SECURITY},
    {"Design", Room::RoomType::DESIGN},
    {"Marketing", Room::RoomType::MARKETING},
    {"HR", Room::RoomType::HR},
    {"Finance", Room::RoomType::FINANCE},
    {"Legal", Room::RoomType::LEGAL},
    {"Customer Support", Room::RoomType::CUSTOMER_SUPPORT},
    {"Other", Room::RoomType::OTHER},
    {"Lobby", Room::RoomType::LOBBY},
};

// privacy to string map
static const std::unordered_map<Room::Privacy, std::string> privacyMap{
    {Room::Privacy::PUBLIC, "PUBLIC"},
    {Room::Privacy::PRIVATE, "PRIVATE"},
};

// string to privacy map
static const std::unordered_map<std::string, Room::Privacy> stringToPrivacyMap{
    {"PUBLIC", Room::Privacy::PUBLIC},
    {"PRIVATE", Room::Privacy::PRIVATE},
};

// constructor
Room::Room(int id, std::string_view name, Room::RoomType type, Room::Privacy privacy)
    : id(id), name(name), type(type), privacy(privacy), created_at(std::time(nullptr)) {}

// stream output operator
std::ostream &operator<<(std::ostream &out, const Room &room) {
  out << "Room " << room.name << " (" << room.id << ", " << room.roomTypeToString(room.type)
      << "):\n";
  out << "created at: " << room.created_at << ", privacy: " << room.privacyToString(room.privacy);
  return out;
}

// type operations
std::string Room::roomTypeToString(Room::RoomType type) {
  if (typeMap.find(type) != typeMap.end()) {
    return typeMap.at(type);
  }
  return typeMap.at(Room::RoomType::OTHER);
}
std::string Room::privacyToString(Room::Privacy privacy) {
  if (privacyMap.find(privacy) != privacyMap.end()) {
    return privacyMap.at(privacy);
  }
  return privacyMap.at(Room::Privacy::PUBLIC);
}
Room::RoomType Room::stringToRoomType(std::string_view type) {
  if (stringToTypeMap.find(std::string(type)) != stringToTypeMap.end()) {
    return stringToTypeMap.at(std::string(type));
  }
  return Room::RoomType::OTHER;
}
Room::Privacy Room::stringToPrivacy(std::string_view privacy) {
  if (stringToPrivacyMap.find(std::string(privacy)) != stringToPrivacyMap.end()) {
    return stringToPrivacyMap.at(std::string(privacy));
  }
  return Room::Privacy::PUBLIC;
}

// getters
int Room::getId() const { return this->id; }
std::string Room::getName() const { return this->name; }
Room::RoomType Room::getType() const { return this->type; }
Room::Privacy Room::getPrivacy() const { return this->privacy; }

// equals operator
bool Room::operator==(const Room &other) const { return this->id == other.id; }
bool Room::operator!=(const Room &other) const { return this->id != other.id; }

// serialize
std::string Room::serialize() const {
  return "room(" + std::to_string(this->id) + "|" + this->name + "|" +
         Room::roomTypeToString(this->type) + "|" + Room::privacyToString(this->privacy) + ")";
}

// deserialize
Room Room::deserialize(const std::string &serialized) {
  static const std::string prefix = "room(";
  if (serialized.size() < prefix.size() + 1 || serialized.compare(0, prefix.size(), prefix) != 0 ||
      serialized.back() != ')') {
    throw std::invalid_argument("Invalid serialized room");
  }

  const std::string body = serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);
  const std::size_t first = body.find('|');
  const std::size_t second =
      (first == std::string::npos) ? std::string::npos : body.find('|', first + 1);
  const std::size_t third =
      (second == std::string::npos) ? std::string::npos : body.find('|', second + 1);
  if (first == std::string::npos || second == std::string::npos || third == std::string::npos) {
    throw std::invalid_argument("Invalid serialized room");
  }

  const std::string idStr = body.substr(0, first);
  const std::string name = body.substr(first + 1, second - first - 1);
  const std::string type = body.substr(second + 1, third - second - 1);
  const std::string privacy = body.substr(third + 1);

  int id = 0;
  try {
    id = std::stoi(idStr);
  } catch (const std::exception &) {
    throw std::invalid_argument("Invalid serialized room id");
  }

  return Room(id, name, Room::stringToRoomType(type), Room::stringToPrivacy(privacy));
}

// serialize a directory
std::string Room::serializeList(const std::vector<Room> &rooms) {
  std::string out;
  for (std::size_t i = 0; i < rooms.size(); ++i) {
    if (i > 0) {
      out.push_back(';');
    }
    out.append(rooms[i].serialize());
  }
  return out;
}

// deserialize a directory
std::vector<Room> Room::deserializeList(const std::string &serialized) {
  std::vector<Room> rooms;
  if (serialized.empty()) {
    return rooms;
  }

  std::size_t start = 0;
  while (start <= serialized.size()) {
    const std::size_t end = serialized.find(';', start);
    const std::string piece =
        (end == std::string::npos) ? serialized.substr(start) : serialized.substr(start, end - start);
    if (!piece.empty()) {
      rooms.push_back(Room::deserialize(piece));
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return rooms;
}
