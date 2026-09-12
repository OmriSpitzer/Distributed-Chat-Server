/**
 * Room header file class
 *
 * @date 11-09-2026
 */

#pragma once
#include <ctime>
#include <string>
#include <string_view>

class Room {
public:
  // privacy type
  enum class Privacy { PUBLIC, PRIVATE };

  // room type
  enum class RoomType {
    RESEARCH_AND_DEVELOPMENT,
    PRODUCTION,
    QA,
    DEVOPS,
    SECURITY,
    DESIGN,
    MARKETING,
    HR,
    FINANCE,
    LEGAL,
    CUSTOMER_SUPPORT,
    OTHER,
    LOBBY
  };

  // constructor
  Room(std::string_view name, Room::RoomType type = RoomType::OTHER,
       Room::Privacy privacy = Privacy::PUBLIC);

  // stream output operator
  friend std::ostream &operator<<(std::ostream &out, const Room &room);

  // type operations
  static std::string roomTypeToString(Room::RoomType type);
  static std::string privacyToString(Room::Privacy privacy);
  static Room::RoomType stringToRoomType(std::string_view type);
  static Room::Privacy stringToPrivacy(std::string_view privacy);

  // getters
  std::string getId() const;
  std::string getName() const;
  Room::RoomType getType() const;
  Room::Privacy getPrivacy() const;

private:
  Room::RoomType type;    // room type
  Room::Privacy privacy;  // privacy type
  std::string id;         // room id
  std::string name;       // room name
  std::time_t created_at; // creation time
};