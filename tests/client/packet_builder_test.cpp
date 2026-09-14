/**
 * PacketBuilder unit tests
 *
 * @brief Includes: login / register / logout / message / join / leave field mapping,
 * empty-argument rejection, receiver and responseCode defaults, timestamp set,
 * whitespace accepted, register email-in-room protocol.
 * @date 13-09-2026
 */

#include "client/packet_builder.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include <string>

/**
 * 1. buildLogin maps fields
 * 2. buildLogin rejects empty arguments
 * 3. buildRegister maps fields (email in room)
 * 4. buildRegister rejects empty arguments
 * 5. buildLogout maps fields from User
 * 6. buildLogout rejects empty username or email
 * 7. buildMessage maps fields
 * 8. buildMessage rejects empty arguments
 * 9. buildJoinRoom maps fields
 * 10. buildLeaveRoom maps fields
 * 11. join / leave reject empty arguments
 * 12. defaults: empty receiver, responseCode 0, timestamp set
 * 13. whitespace-only arguments are accepted
 */

// 1. buildLogin maps fields
TEST_CASE("PacketBuilder buildLogin maps fields", "[packet_builder][login]") {
  const Packet packet = PacketBuilder::buildLogin("alice", "secret");

  REQUIRE(packet.type == Packet::PacketType::LOGIN);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.message == "secret");
  REQUIRE(packet.room.empty());
  REQUIRE(packet.receiver.empty());
}

// 2. buildLogin rejects empty arguments
TEST_CASE("PacketBuilder buildLogin rejects empty arguments", "[packet_builder][login][edge]") {
  SECTION("empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildLogin("", "secret"), std::invalid_argument);
  }
  SECTION("empty password") {
    REQUIRE_THROWS_AS(PacketBuilder::buildLogin("alice", ""), std::invalid_argument);
  }
  SECTION("both empty") {
    REQUIRE_THROWS_AS(PacketBuilder::buildLogin("", ""), std::invalid_argument);
  }
}

// 3. buildRegister maps fields (email in room)
TEST_CASE("PacketBuilder buildRegister maps fields", "[packet_builder][register]") {
  const Packet packet = PacketBuilder::buildRegister("alice", "secret", "alice@example.com");

  REQUIRE(packet.type == Packet::PacketType::REGISTER);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.message == "secret");
  REQUIRE(packet.room == "alice@example.com");
  REQUIRE(packet.receiver.empty());
}

// 4. buildRegister rejects empty arguments
TEST_CASE("PacketBuilder buildRegister rejects empty arguments",
          "[packet_builder][register][edge]") {
  SECTION("empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildRegister("", "secret", "a@b.com"),
                      std::invalid_argument);
  }
  SECTION("empty password") {
    REQUIRE_THROWS_AS(PacketBuilder::buildRegister("alice", "", "a@b.com"),
                      std::invalid_argument);
  }
  SECTION("empty email") {
    REQUIRE_THROWS_AS(PacketBuilder::buildRegister("alice", "secret", ""),
                      std::invalid_argument);
  }
}

// 5. buildLogout maps fields from User
TEST_CASE("PacketBuilder buildLogout maps fields from User", "[packet_builder][logout]") {
  const User user("alice", "alice@example.com", User::UserType::USER);
  const Packet packet = PacketBuilder::buildLogout(user);

  REQUIRE(packet.type == Packet::PacketType::LOGOUT);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.message == "alice@example.com");
  REQUIRE(packet.room.empty());
  REQUIRE(packet.receiver.empty());
}

// 6. buildLogout rejects empty username or email
TEST_CASE("PacketBuilder buildLogout rejects empty username or email",
          "[packet_builder][logout][edge]") {
  SECTION("empty username") {
    const User user("", "alice@example.com", User::UserType::USER);
    REQUIRE_THROWS_AS(PacketBuilder::buildLogout(user), std::invalid_argument);
  }
  SECTION("empty email") {
    const User user("alice", "", User::UserType::USER);
    REQUIRE_THROWS_AS(PacketBuilder::buildLogout(user), std::invalid_argument);
  }
}

// 7. buildMessage maps fields
TEST_CASE("PacketBuilder buildMessage maps fields", "[packet_builder][message]") {
  const Packet packet = PacketBuilder::buildMessage("alice", "hello");

  REQUIRE(packet.type == Packet::PacketType::MESSAGE);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.message == "hello");
  REQUIRE(packet.room.empty());
}

// 8. buildMessage rejects empty arguments
TEST_CASE("PacketBuilder buildMessage rejects empty arguments",
          "[packet_builder][message][edge]") {
  SECTION("empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildMessage("", "hello"), std::invalid_argument);
  }
  SECTION("empty message") {
    REQUIRE_THROWS_AS(PacketBuilder::buildMessage("alice", ""), std::invalid_argument);
  }
}

// 9. buildJoinRoom maps fields
TEST_CASE("PacketBuilder buildJoinRoom maps fields", "[packet_builder][join]") {
  const Packet packet = PacketBuilder::buildJoinRoom("alice", "General");

  REQUIRE(packet.type == Packet::PacketType::ROOM_JOIN);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.room == "General");
  REQUIRE(packet.message.empty());
}

// 10. buildLeaveRoom maps fields
TEST_CASE("PacketBuilder buildLeaveRoom maps fields", "[packet_builder][leave]") {
  const Packet packet = PacketBuilder::buildLeaveRoom("alice", "General");

  REQUIRE(packet.type == Packet::PacketType::ROOM_LEAVE);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.room == "General");
  REQUIRE(packet.message.empty());
}

// 11. join / leave reject empty arguments
TEST_CASE("PacketBuilder join and leave reject empty arguments",
          "[packet_builder][join][leave][edge]") {
  SECTION("join empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildJoinRoom("", "General"), std::invalid_argument);
  }
  SECTION("join empty room") {
    REQUIRE_THROWS_AS(PacketBuilder::buildJoinRoom("alice", ""), std::invalid_argument);
  }
  SECTION("leave empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildLeaveRoom("", "General"), std::invalid_argument);
  }
  SECTION("leave empty room") {
    REQUIRE_THROWS_AS(PacketBuilder::buildLeaveRoom("alice", ""), std::invalid_argument);
  }
}

// 12. buildUpdateUser maps fields and allows empty password
TEST_CASE("PacketBuilder buildUpdateUser maps fields", "[packet_builder][update]") {
  SECTION("username and password change") {
    const Packet packet = PacketBuilder::buildUpdateUser("alice", "newpw", "alice@example.com");
    REQUIRE(packet.type == Packet::PacketType::UPDATE_USER);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.message == "newpw");
    REQUIRE(packet.room == "alice@example.com");
  }

  SECTION("empty password keeps current on server") {
    const Packet packet = PacketBuilder::buildUpdateUser("bob", "", "bob@example.com");
    REQUIRE(packet.type == Packet::PacketType::UPDATE_USER);
    REQUIRE(packet.sender == "bob");
    REQUIRE(packet.message.empty());
    REQUIRE(packet.room == "bob@example.com");
  }
}

TEST_CASE("PacketBuilder buildUpdateUser rejects empty username or email",
          "[packet_builder][update][edge]") {
  SECTION("empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildUpdateUser("", "pw", "a@b.com"), std::invalid_argument);
  }
  SECTION("empty email") {
    REQUIRE_THROWS_AS(PacketBuilder::buildUpdateUser("alice", "pw", ""), std::invalid_argument);
  }
}

// 14. buildCreateRoom maps fields
TEST_CASE("PacketBuilder buildCreateRoom maps fields", "[packet_builder][create]") {
  SECTION("defaults empty type/privacy") {
    const Packet packet = PacketBuilder::buildCreateRoom("alice", "Labs");
    REQUIRE(packet.type == Packet::PacketType::ROOM_CREATE);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.room == "Labs");
    REQUIRE(packet.message.empty());
  }

  SECTION("explicit type and privacy") {
    const Packet packet = PacketBuilder::buildCreateRoom("alice", "Secure", "Security|PRIVATE");
    REQUIRE(packet.type == Packet::PacketType::ROOM_CREATE);
    REQUIRE(packet.message == "Security|PRIVATE");
  }
}

TEST_CASE("PacketBuilder buildCreateRoom rejects empty arguments",
          "[packet_builder][create][edge]") {
  SECTION("empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildCreateRoom("", "Labs"), std::invalid_argument);
  }
  SECTION("empty room") {
    REQUIRE_THROWS_AS(PacketBuilder::buildCreateRoom("alice", ""), std::invalid_argument);
  }
}

TEST_CASE("PacketBuilder buildLoadMessageHistory maps fields", "[packet_builder][history]") {
  const Packet packet = PacketBuilder::buildLoadMessageHistory("alice", "Lobby");
  REQUIRE(packet.type == Packet::PacketType::LOAD_MESSAGE_HISTORY);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.room == "Lobby");
  REQUIRE(packet.message.empty());
}

TEST_CASE("PacketBuilder buildLoadMessageHistory rejects empty arguments",
          "[packet_builder][history][edge]") {
  SECTION("empty username") {
    REQUIRE_THROWS_AS(PacketBuilder::buildLoadMessageHistory("", "Lobby"),
                      std::invalid_argument);
  }
  SECTION("empty room") {
    REQUIRE_THROWS_AS(PacketBuilder::buildLoadMessageHistory("alice", ""),
                      std::invalid_argument);
  }
}

// 13. defaults: empty receiver, responseCode 0, timestamp set
TEST_CASE("PacketBuilder sets defaults and timestamp", "[packet_builder][edge]") {
  const Packet packet = PacketBuilder::buildLogin("alice", "secret");

  REQUIRE(packet.receiver.empty());
  REQUIRE(packet.responseCode == 0);
  REQUIRE(packet.timestamp > 0);
}

// 13. whitespace-only arguments are accepted
TEST_CASE("PacketBuilder accepts whitespace-only arguments", "[packet_builder][edge]") {
  const Packet login = PacketBuilder::buildLogin(" ", " ");
  REQUIRE(login.sender == " ");
  REQUIRE(login.message == " ");

  const Packet join = PacketBuilder::buildJoinRoom("alice", " ");
  REQUIRE(join.room == " ");
}
