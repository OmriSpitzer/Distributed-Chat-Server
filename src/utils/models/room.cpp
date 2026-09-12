/**
 * Room class
 *
 * @brief Room class to store a room and its metadata.
 * @date 11-09-2026
 */
#include "utils/models/room.h"
#include <atomic>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {
std::atomic<uint64_t> next_room_id{0};
}

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
Room::Room(std::string_view name, Room::RoomType type, Room::Privacy privacy)
    : id(std::to_string(++next_room_id)), name(name), type(type), privacy(privacy),
      created_at(std::time(nullptr)) {}

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
std::string Room::getId() const { return this->id; }
std::string Room::getName() const { return this->name; }
Room::RoomType Room::getType() const { return this->type; }
Room::Privacy Room::getPrivacy() const { return this->privacy; }