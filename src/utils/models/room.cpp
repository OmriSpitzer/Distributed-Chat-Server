/**
 * Room class implementation file
 *
 * @brief Room class to store a room and its metadata.
 * @date 11-09-2026
 *
 * Room class with fields: id, name, type, privacy, created_at, creator_email
 * Used for storing and displaying rooms in the server
 * Serialized format: room(id|name|type|privacy)
 * Serialized list format: room1;room2;room3;...
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

// privacy to string map
static const std::unordered_map<Room::Privacy, std::string> privacyMap{
    {Room::Privacy::PUBLIC, "PUBLIC"},
    {Room::Privacy::PRIVATE, "PRIVATE"},
};

// constructor
Room::Room(int id, std::string_view name, Room::RoomType type, Room::Privacy privacy)
    : id(id), name(name), type(type), privacy(privacy), created_at(std::time(nullptr)),
      creator_email() {}

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
  for (const auto &[key, value] : typeMap) {
    if (value == type)
      return key;
  }
  return RoomType::OTHER;
}
Room::Privacy Room::stringToPrivacy(std::string_view privacy) {
  for (const auto &[key, value] : privacyMap) {
    if (value == privacy)
      return key;
  }
  return Privacy::PUBLIC;
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

  // check correct serialization format
  if (serialized.size() < prefix.size() + 1 || serialized.compare(0, prefix.size(), prefix) != 0 ||
      serialized.back() != ')') {
    throw std::invalid_argument("Invalid serialized room");
  }

  // extract the body of the serialized room
  const std::string body = serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);
  const std::size_t first = body.find('|');
  const std::size_t second =
      (first == std::string::npos) ? std::string::npos : body.find('|', first + 1);
  const std::size_t third =
      (second == std::string::npos) ? std::string::npos : body.find('|', second + 1);

  // check if all the fields are present
  if (first == std::string::npos || second == std::string::npos || third == std::string::npos)
    throw std::invalid_argument("Invalid serialized room");

  // extract the fields from the body
  const std::string idStr = body.substr(0, first);
  const std::string name = body.substr(first + 1, second - first - 1);
  const std::string type = body.substr(second + 1, third - second - 1);
  const std::string privacy = body.substr(third + 1);

  // convert the id string to an integer
  int id = 0;
  try {
    id = std::stoi(idStr);
  } catch (const std::exception &) {
    throw std::invalid_argument("Invalid serialized room id");
  }

  // create the room object
  return Room(id, name, Room::stringToRoomType(type), Room::stringToPrivacy(privacy));
}

// serialize a list of rooms
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

// deserialize a list of rooms
std::vector<Room> Room::deserializeList(const std::string &serialized) {
  std::vector<Room> rooms;

  // check if the serialized list is empty
  if (serialized.empty()) {
    return rooms;
  }

  // extract the rooms from the serialized list
  std::size_t start = 0;
  while (start <= serialized.size()) {
    const std::size_t end = serialized.find(';', start);
    const std::string piece = (end == std::string::npos) ? serialized.substr(start)
                                                         : serialized.substr(start, end - start);

    // check if the piece is not empty
    if (!piece.empty()) {
      try {
        rooms.push_back(Room::deserialize(piece));
      } catch (const std::exception &) {
        // skip malformed entries so one bad room does not wipe the directory
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return rooms;
}
