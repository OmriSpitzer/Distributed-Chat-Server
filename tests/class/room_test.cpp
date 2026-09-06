/**
 * Room Class unit tests
 */

#include "utils/models/room.h"
#include "utils/models/user.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

static User alice("alice", "alice@example.com", User::UserType::USER);
static User bob("bob", "bob@example.com", User::UserType::USER);

TEST_CASE("Room stores name, type, and privacy", "[room][ctor]") {
  SECTION("defaults") {
    const Room room("general");
    REQUIRE(room.getName() == "general");
    REQUIRE(room.getType() == Room::RoomType::OTHER);
    REQUIRE(room.getPrivacy() == Room::Privacy::PUBLIC);
    REQUIRE_FALSE(room.getId().empty());
  }

  SECTION("explicit type and privacy") {
    const Room room("secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);
    REQUIRE(room.getName() == "secure");
    REQUIRE(room.getType() == Room::RoomType::SECURITY);
    REQUIRE(room.getPrivacy() == Room::Privacy::PRIVATE);
  }

  SECTION("empty name") {
    const Room room("");
    REQUIRE(room.getName().empty());
    REQUIRE_FALSE(room.getId().empty());
  }
}

TEST_CASE("Successive rooms receive unique ids", "[room][id]") {
  const Room first("one");
  const Room second("two");

  REQUIRE(first.getId() != second.getId());
}

TEST_CASE("Room user membership", "[room][users]") {
  Room room("general");

  SECTION("add and get") {
    REQUIRE(room.isEmpty());
    REQUIRE(room.addUser(alice));
    REQUIRE_FALSE(room.isEmpty());
    REQUIRE(room.getUser("alice@example.com") != nullptr);
    REQUIRE(*room.getUser("alice@example.com") == alice);
  }

  SECTION("duplicate email is rejected") {
    REQUIRE(room.addUser(alice));
    REQUIRE_FALSE(room.addUser(alice));
  }

  SECTION("unknown user") {
    REQUIRE(room.getUser("missing@example.com") == nullptr);
    REQUIRE_FALSE(room.removeUser("missing@example.com"));
  }

  SECTION("remove user") {
    REQUIRE(room.addUser(alice));
    REQUIRE(room.removeUser("alice@example.com"));
    REQUIRE(room.isEmpty());
    REQUIRE(room.getUser("alice@example.com") == nullptr);
  }

  SECTION("capacity of 20") {
    for (int i = 0; i < 20; ++i) {
      User user("user" + std::to_string(i), "user" + std::to_string(i) + "@example.com",
                User::UserType::USER);
      REQUIRE(room.addUser(user));
    }
    User extra("extra", "extra@example.com", User::UserType::USER);
    REQUIRE_FALSE(room.addUser(extra));
  }
}

TEST_CASE("Room ping reflects occupancy", "[room][ping]") {
  Room room("general");

  SECTION("empty") {
    REQUIRE_FALSE(room.ping());
  }

  SECTION("occupied") {
    REQUIRE(room.addUser(alice));
    REQUIRE(room.ping());
  }
}

TEST_CASE("Room accepts messages", "[room][messages]") {
  Room room("general");

  SECTION("plain text") {
    REQUIRE(room.addMessage(alice, bob, "hello"));
  }

  SECTION("empty content") {
    REQUIRE(room.addMessage(alice, bob, ""));
  }

  SECTION("multiline content") {
    REQUIRE(room.addMessage(alice, bob, "line1\nline2"));
  }
}

TEST_CASE("Room stream output includes name, type, and privacy", "[room][print]") {
  SECTION("public other") {
    Room room("general");
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("general") != std::string::npos);
    REQUIRE(text.find(room.getId()) != std::string::npos);
    REQUIRE(text.find("Other") != std::string::npos);
    REQUIRE(text.find("PUBLIC") != std::string::npos);
  }

  SECTION("private security") {
    Room room("secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("secure") != std::string::npos);
    REQUIRE(text.find("Security") != std::string::npos);
    REQUIRE(text.find("PRIVATE") != std::string::npos);
  }
}
