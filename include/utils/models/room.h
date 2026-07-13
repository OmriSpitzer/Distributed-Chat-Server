/**
 * Room header file class
 *
 * @date 13-07-2026
 */

#pragma once

#include "utils/models/message.h"
#include "utils/models/user.h"
#include <ctime>
#include <map>
#include <string>
#include <vector>

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
    OTHER
  };

  // constructor
  Room(std::string_view name, Room::RoomType type = RoomType::OTHER,
       Room::Privacy privacy = Privacy::PUBLIC);

  // stream output operator
  friend std::ostream &operator<<(std::ostream &out, Room &room);

  // user operations
  bool addUser(User &user);
  bool addMessage(User &from, User &to, std::string_view message);

  // authorization operations
  bool removeUser(std::string_view email, std::string_view reason = "",
                  std::string_view autherization = "");
  User *getUser(std::string_view email, std::string_view authorization = "");
  bool isEmpty(std::string_view authorization = "");

  // health check
  bool ping();

  // destructor
  ~Room();

  // getters
  std::string roomTypeToString();
  std::string privacyToString();

private:
  std::string id;                    // room id
  std::string name;                  // room name
  Room::RoomType type;               // room type
  Room::Privacy privacy;             // privacy type
  std::map<std::string, User> users; // users in the room
  std::vector<Message> history;      // message history
  std::time_t created_at;            // creation time
  int num_users;                     // number of users in the room
  int num_messages;                  // number of messages in the room
  int ROOM_CAPACITY = 20;            // room capacity
};