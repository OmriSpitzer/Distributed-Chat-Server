/**
 * Room test file
 *
 * @brief Test cases for the Room class
 * @date 13-07-2026
 */

#include "utils/models/room.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string captureStream(Room &room) {
  std::ostringstream stream;
  stream << room;
  return stream.str();
}

User makeUser(const std::string &name) {
  return User(name, name + "@example.com", User::UserType::USER);
}

} // namespace

TEST_CASE("Room constructor defaults", "[Room][constructor]") {
  Room room("general");

  REQUIRE(room.isEmpty());
  REQUIRE_FALSE(room.ping());
  REQUIRE(room.roomTypeToString() == "Other");
  REQUIRE(room.privacyToString() == "PUBLIC");
  REQUIRE(room.getUser("nobody@example.com") == nullptr);
}

TEST_CASE("Room constructor with type and privacy", "[Room][constructor]") {
  Room room("ops", Room::RoomType::DEVOPS, Room::Privacy::PRIVATE);

  REQUIRE(room.roomTypeToString() == "DevOps");
  REQUIRE(room.privacyToString() == "PRIVATE");
  REQUIRE(room.isEmpty());
}

TEST_CASE("Room type labels", "[Room][getters][types]") {
  SECTION("R&D") {
    Room room("rd", Room::RoomType::RESEARCH_AND_DEVELOPMENT);
    REQUIRE(room.roomTypeToString() == "R&D");
  }
  SECTION("Production") {
    Room room("prod", Room::RoomType::PRODUCTION);
    REQUIRE(room.roomTypeToString() == "Production");
  }
  SECTION("QA") {
    Room room("qa", Room::RoomType::QA);
    REQUIRE(room.roomTypeToString() == "QA");
  }
  SECTION("Security") {
    Room room("sec", Room::RoomType::SECURITY);
    REQUIRE(room.roomTypeToString() == "Security");
  }
  SECTION("Customer Support") {
    Room room("support", Room::RoomType::CUSTOMER_SUPPORT);
    REQUIRE(room.roomTypeToString() == "Customer Support");
  }
  SECTION("Other") {
    Room room("misc", Room::RoomType::OTHER);
    REQUIRE(room.roomTypeToString() == "Other");
  }
}

TEST_CASE("Room stream output", "[Room][ostream]") {
  Room room("lobby", Room::RoomType::DESIGN, Room::Privacy::PUBLIC);
  const std::string output = captureStream(room);

  REQUIRE(output.find("Room lobby") != std::string::npos);
  REQUIRE(output.find("Design") != std::string::npos);
  REQUIRE(output.find("PUBLIC") != std::string::npos);
  REQUIRE(output.find("0 users") != std::string::npos);
  REQUIRE(output.find("0 messages") != std::string::npos);
}

TEST_CASE("Room addUser and getUser", "[Room][users]") {
  Room room("chat");
  User alice = makeUser("alice");
  User bob = makeUser("bob");

  REQUIRE(room.addUser(alice));
  REQUIRE_FALSE(room.isEmpty());
  REQUIRE(room.ping());

  User *found = room.getUser("alice@example.com");
  REQUIRE(found != nullptr);
  REQUIRE(found->getUsername() == "alice");
  REQUIRE(found->getEmail() == "alice@example.com");

  REQUIRE(room.addUser(bob));
  REQUIRE(room.getUser("bob@example.com") != nullptr);
  REQUIRE(room.getUser("missing@example.com") == nullptr);
}

TEST_CASE("Room rejects duplicate user email", "[Room][users][edge]") {
  Room room("chat");
  User alice = makeUser("alice");
  User aliceDuplicate("alice2", "alice@example.com", User::UserType::GUEST);

  REQUIRE(room.addUser(alice));
  REQUIRE_FALSE(room.addUser(aliceDuplicate));

  User *found = room.getUser("alice@example.com");
  REQUIRE(found != nullptr);
  REQUIRE(found->getUsername() == "alice");
  REQUIRE(found->getUserType() == User::UserType::USER);
}

TEST_CASE("Room rejects users beyond capacity", "[Room][users][capacity]") {
  Room room("full");
  std::vector<User> users;
  users.reserve(21);

  for (int i = 0; i < 20; ++i) {
    users.push_back(makeUser("user" + std::to_string(i)));
    REQUIRE(room.addUser(users.back()));
  }

  users.push_back(makeUser("overflow"));
  REQUIRE_FALSE(room.addUser(users.back()));
  REQUIRE(room.getUser("overflow@example.com") == nullptr);
  REQUIRE(captureStream(room).find("20 users") != std::string::npos);
}

TEST_CASE("Room removeUser", "[Room][users]") {
  Room room("chat");
  User alice = makeUser("alice");
  User bob = makeUser("bob");

  REQUIRE(room.addUser(alice));
  REQUIRE(room.addUser(bob));
  REQUIRE(room.removeUser("alice@example.com"));
  REQUIRE(room.getUser("alice@example.com") == nullptr);
  REQUIRE(room.getUser("bob@example.com") != nullptr);
  REQUIRE_FALSE(room.isEmpty());

  REQUIRE(room.removeUser("bob@example.com", "left"));
  REQUIRE(room.isEmpty());
  REQUIRE_FALSE(room.ping());
  REQUIRE_FALSE(room.removeUser("bob@example.com"));
}

TEST_CASE("Room addMessage updates count in stream", "[Room][messages]") {
  Room room("chat");
  User alice = makeUser("alice");
  User bob = makeUser("bob");

  REQUIRE(room.addUser(alice));
  REQUIRE(room.addUser(bob));
  REQUIRE(room.addMessage(alice, bob, "hello"));
  REQUIRE(room.addMessage(bob, alice, "hi back"));

  const std::string output = captureStream(room);
  REQUIRE(output.find("2 users") != std::string::npos);
  REQUIRE(output.find("2 messages") != std::string::npos);
}

TEST_CASE("Room ping reflects occupancy", "[Room][ping]") {
  Room room("status");
  User alice = makeUser("alice");

  REQUIRE_FALSE(room.ping());
  REQUIRE(room.addUser(alice));
  REQUIRE(room.ping());
  REQUIRE(room.removeUser("alice@example.com"));
  REQUIRE_FALSE(room.ping());
}
