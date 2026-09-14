/**
 * ClientState unit tests
 *
 * @brief Includes: default empty, isLoggedIn from user only, clear empty / after session,
 * room without user, user without room, replace user / room, clear is idempotent,
 * anonymous / empty-field user, Lobby room.
 * @date 13-09-2026
 */

#include "client/client_state.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>

/**
 * 1. default state is empty
 * 2. isLoggedIn follows user only
 * 3. clear on empty state
 * 4. clear after login session
 * 5. room without user is not logged in
 * 6. user without room is logged in
 * 7. replace user and room
 * 8. clear is idempotent
 * 9. anonymous and empty-field users
 * 10. Lobby room assignment
 */

// 1. default state is empty
TEST_CASE("ClientState default state is empty", "[client_state][ctor][edge]") {
  const ClientState state;

  REQUIRE_FALSE(state.user.has_value());
  REQUIRE_FALSE(state.currentRoom.has_value());
  REQUIRE_FALSE(state.isLoggedIn());
}

// 2. isLoggedIn follows user only
TEST_CASE("ClientState isLoggedIn follows user only", "[client_state][login][edge]") {
  ClientState state;

  state.user = User("alice", "alice@example.com", User::UserType::USER);
  REQUIRE(state.isLoggedIn());

  state.user.reset();
  REQUIRE_FALSE(state.isLoggedIn());
}

// 3. clear on empty state
TEST_CASE("ClientState clear on empty state", "[client_state][clear][edge]") {
  ClientState state;
  state.clear();

  REQUIRE_FALSE(state.user.has_value());
  REQUIRE_FALSE(state.currentRoom.has_value());
  REQUIRE_FALSE(state.isLoggedIn());
}

// 4. clear after login session
TEST_CASE("ClientState clear after login session", "[client_state][clear]") {
  ClientState state;
  state.user = User("alice", "alice@example.com", User::UserType::USER);
  state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);

  REQUIRE(state.isLoggedIn());
  state.clear();

  REQUIRE_FALSE(state.user.has_value());
  REQUIRE_FALSE(state.currentRoom.has_value());
  REQUIRE_FALSE(state.isLoggedIn());
}

// 5. room without user is not logged in
TEST_CASE("ClientState room without user is not logged in", "[client_state][edge]") {
  ClientState state;
  state.currentRoom = Room(2, "General", Room::RoomType::OTHER);

  REQUIRE(state.currentRoom.has_value());
  REQUIRE_FALSE(state.user.has_value());
  REQUIRE_FALSE(state.isLoggedIn());
}

// 6. user without room is logged in
TEST_CASE("ClientState user without room is logged in", "[client_state][edge]") {
  ClientState state;
  state.user = User("bob", "bob@example.com", User::UserType::ADMIN);

  REQUIRE(state.isLoggedIn());
  REQUIRE_FALSE(state.currentRoom.has_value());
}

// 7. replace user and room
TEST_CASE("ClientState replace user and room", "[client_state][edge]") {
  ClientState state;
  state.user = User("alice", "alice@example.com", User::UserType::USER);
  state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);

  state.user = User("bob", "bob@example.com", User::UserType::ADMIN);
  state.currentRoom = Room(3, "secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);

  REQUIRE(state.isLoggedIn());
  REQUIRE(state.user->getUsername() == "bob");
  REQUIRE(state.user->getEmail() == "bob@example.com");
  REQUIRE(state.user->getUserType() == User::UserType::ADMIN);
  REQUIRE(state.currentRoom->getName() == "secure");
  REQUIRE(state.currentRoom->getType() == Room::RoomType::SECURITY);
  REQUIRE(state.currentRoom->getPrivacy() == Room::Privacy::PRIVATE);
}

// 8. clear is idempotent
TEST_CASE("ClientState clear is idempotent", "[client_state][clear][edge]") {
  ClientState state;
  state.user = User("alice", "alice@example.com", User::UserType::USER);
  state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);

  state.clear();
  state.clear();

  REQUIRE_FALSE(state.isLoggedIn());
  REQUIRE_FALSE(state.user.has_value());
  REQUIRE_FALSE(state.currentRoom.has_value());
}

// 9. anonymous and empty-field users
TEST_CASE("ClientState anonymous and empty-field users", "[client_state][user][edge]") {
  ClientState state;

  SECTION("anonymous user") {
    const User anon = User::anonymousUser();
    state.user = anon;
    REQUIRE(state.isLoggedIn());
    REQUIRE(state.user->getUsername() == anon.getUsername());
    REQUIRE(state.user->getEmail() == anon.getEmail());
    REQUIRE(state.user->getUserType() == User::UserType::GUEST);
    REQUIRE(state.user->getUsername().rfind("anon", 0) == 0);
  }

  SECTION("empty username and email") {
    state.user = User("", "", User::UserType::GUEST);
    REQUIRE(state.isLoggedIn());
    REQUIRE(state.user->getUsername().empty());
    REQUIRE(state.user->getEmail().empty());
  }
}

// 10. Lobby room assignment
TEST_CASE("ClientState Lobby room assignment", "[client_state][room][edge]") {
  ClientState state;
  state.user = User("alice", "alice@example.com", User::UserType::USER);
  state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);

  REQUIRE(state.isLoggedIn());
  REQUIRE(state.currentRoom->getName() == "Lobby");
  REQUIRE(state.currentRoom->getType() == Room::RoomType::LOBBY);

  state.currentRoom = Room(0, "", Room::RoomType::OTHER);
  REQUIRE(state.currentRoom->getName().empty());
  REQUIRE(state.isLoggedIn());
}
