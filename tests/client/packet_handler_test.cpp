/**
 * PacketHandler unit tests
 *
 * @brief Includes: successful login / register deserialize, non-SUCCESS rejection,
 * invalid payload, non-auth packet types return nullopt, responseCode edge values.
 * @date 13-09-2026
 */

#include "client/packet_handler.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>
#include <optional>

/**
 * 1. LOGIN success deserializes user
 * 2. REGISTER success deserializes user
 * 3. LOGIN / REGISTER reject non-SUCCESS
 * 4. LOGIN / REGISTER reject invalid payload
 * 5. non-auth packet types return nullopt
 * 6. SUCCESS with empty message fails
 * 7. responseCode edge values
 */

namespace {

Packet authPacket(Packet::PacketType type, int responseCode, const std::string &message) {
  Packet packet;
  packet.type = type;
  packet.responseCode = responseCode;
  packet.message = message;
  return packet;
}

} // namespace

// 1. LOGIN success deserializes user
TEST_CASE("PacketHandler LOGIN success deserializes user", "[packet_handler][login]") {
  PacketHandler handler;
  const User expected("alice", "alice@example.com", User::UserType::USER);
  const Packet packet =
      authPacket(Packet::PacketType::LOGIN, static_cast<int>(RESPONSE_CODES::SUCCESS),
                 expected.serialize());

  const std::optional<User> result = handler.handlePacket(packet);

  REQUIRE(result.has_value());
  REQUIRE(result->getUsername() == "alice");
  REQUIRE(result->getEmail() == "alice@example.com");
  REQUIRE(result->getUserType() == User::UserType::USER);
}

// 2. REGISTER success deserializes user
TEST_CASE("PacketHandler REGISTER success deserializes user", "[packet_handler][register]") {
  PacketHandler handler;
  const User expected("bob", "bob@example.com", User::UserType::ADMIN);
  const Packet packet =
      authPacket(Packet::PacketType::REGISTER, static_cast<int>(RESPONSE_CODES::SUCCESS),
                 expected.serialize());

  const std::optional<User> result = handler.handlePacket(packet);

  REQUIRE(result.has_value());
  REQUIRE(result->getUsername() == "bob");
  REQUIRE(result->getEmail() == "bob@example.com");
  REQUIRE(result->getUserType() == User::UserType::ADMIN);
}

// 3. LOGIN / REGISTER reject non-SUCCESS
TEST_CASE("PacketHandler rejects non-SUCCESS auth responses", "[packet_handler][edge]") {
  PacketHandler handler;
  const std::string payload = User("alice", "a@b.com", User::UserType::USER).serialize();

  SECTION("LOGIN ERROR") {
    const Packet packet =
        authPacket(Packet::PacketType::LOGIN, static_cast<int>(RESPONSE_CODES::ERROR), payload);
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("REGISTER NOT_FOUND") {
    const Packet packet = authPacket(Packet::PacketType::REGISTER,
                                     static_cast<int>(RESPONSE_CODES::NOT_FOUND), payload);
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("LOGIN INTERNAL_SERVER_ERROR") {
    const Packet packet = authPacket(Packet::PacketType::LOGIN,
                                     static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR),
                                     "login failed");
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
}

// 4. LOGIN / REGISTER reject invalid payload
TEST_CASE("PacketHandler rejects invalid auth payload", "[packet_handler][edge]") {
  PacketHandler handler;

  SECTION("LOGIN malformed") {
    const Packet packet =
        authPacket(Packet::PacketType::LOGIN, static_cast<int>(RESPONSE_CODES::SUCCESS),
                   "not-a-user");
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("REGISTER incomplete serialize") {
    const Packet packet =
        authPacket(Packet::PacketType::REGISTER, static_cast<int>(RESPONSE_CODES::SUCCESS),
                   "user(alice|only-one-field)");
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
}

// 5. non-auth packet types return nullopt
TEST_CASE("PacketHandler ignores non-auth packet types", "[packet_handler][edge]") {
  PacketHandler handler;

  SECTION("MESSAGE") {
    Packet packet;
    packet.type = Packet::PacketType::MESSAGE;
    packet.responseCode = 0;
    packet.sender = "alice";
    packet.message = "hello";
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("LOGOUT") {
    Packet packet;
    packet.type = Packet::PacketType::LOGOUT;
    packet.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("ROOM_JOIN") {
    Packet packet;
    packet.type = Packet::PacketType::ROOM_JOIN;
    packet.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    packet.room = "General";
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("HEARTBEAT") {
    Packet packet;
    packet.type = Packet::PacketType::HEARTBEAT;
    packet.message = "ping";
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
}

// 6. SUCCESS with empty message fails
TEST_CASE("PacketHandler SUCCESS with empty message fails", "[packet_handler][edge]") {
  PacketHandler handler;
  const Packet packet =
      authPacket(Packet::PacketType::LOGIN, static_cast<int>(RESPONSE_CODES::SUCCESS), "");

  REQUIRE_FALSE(handler.handlePacket(packet).has_value());
}

// 7. responseCode edge values
TEST_CASE("PacketHandler responseCode edge values", "[packet_handler][edge]") {
  PacketHandler handler;
  const std::string payload = User("alice", "a@b.com", User::UserType::USER).serialize();

  SECTION("zero is not SUCCESS") {
    const Packet packet = authPacket(Packet::PacketType::LOGIN, 0, payload);
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("199 is not SUCCESS") {
    const Packet packet = authPacket(Packet::PacketType::LOGIN, 199, payload);
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
  SECTION("exact SUCCESS 200") {
    const Packet packet = authPacket(Packet::PacketType::LOGIN, 200, payload);
    REQUIRE(handler.handlePacket(packet).has_value());
  }
}
