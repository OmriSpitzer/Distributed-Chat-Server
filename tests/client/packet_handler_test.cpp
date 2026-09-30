/**
 * PacketHandler unit tests
 *
 * @brief Includes: successful login / register / profile-update deserialize, non-SUCCESS
 * rejection, invalid payload, every other packet type returns nullopt, responseCode edges.
 * @date 13-09-2026
 */

#include "client/packet_handler.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>
#include <optional>
#include <vector>

/**
 * 1. LOGIN success deserializes user
 * 2. REGISTER success deserializes user
 * 3. UPDATE_USER success deserializes user
 * 4. LOGIN / REGISTER / UPDATE_USER reject non-SUCCESS
 * 5. LOGIN / REGISTER / UPDATE_USER reject invalid payload
 * 6. every other packet type returns nullopt
 * 7. SUCCESS with empty message fails
 * 8. responseCode edge values
 */

namespace {

Packet authPacket(Packet::PacketType type, int responseCode, const std::string &message) {
  Packet packet;
  packet.type = type;
  packet.responseCode = responseCode;
  packet.message = message;
  return packet;
}

const std::vector<Packet::PacketType> &allPacketTypes() {
  static const std::vector<Packet::PacketType> types = {
      Packet::PacketType::LOGIN,
      Packet::PacketType::LOGOUT,
      Packet::PacketType::MESSAGE,
      Packet::PacketType::ROOM_JOIN,
      Packet::PacketType::ROOM_LEAVE,
      Packet::PacketType::DEFAULT,
      Packet::PacketType::HEARTBEAT,
      Packet::PacketType::REGISTER,
      Packet::PacketType::GOSSIP_HELLO,
      Packet::PacketType::GOSSIP_EVENT,
      Packet::PacketType::GOSSIP_DIGEST,
      Packet::PacketType::GOSSIP_PULL,
      Packet::PacketType::UPDATE_USER,
      Packet::PacketType::ROOM_CREATE,
      Packet::PacketType::ROOM_LIST,
      Packet::PacketType::LOAD_MESSAGE_HISTORY,
      Packet::PacketType::ROOM_INVITE,
      Packet::PacketType::ROOM_DELETE,
      Packet::PacketType::ROOM_KICK,
      Packet::PacketType::SERVER_DIRECTORY,
      Packet::PacketType::RECONNECT,
  };
  return types;
}

bool returnsUser(Packet::PacketType type) {
  return type == Packet::PacketType::LOGIN || type == Packet::PacketType::REGISTER ||
         type == Packet::PacketType::UPDATE_USER;
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

// 3. UPDATE_USER success deserializes user
TEST_CASE("PacketHandler UPDATE_USER success deserializes user", "[packet_handler][update]") {
  PacketHandler handler;
  const User expected("carol", "carol@example.com", User::UserType::USER);
  const Packet packet =
      authPacket(Packet::PacketType::UPDATE_USER, static_cast<int>(RESPONSE_CODES::SUCCESS),
                 expected.serialize());

  const std::optional<User> result = handler.handlePacket(packet);

  REQUIRE(result.has_value());
  REQUIRE(result->getUsername() == "carol");
  REQUIRE(result->getEmail() == "carol@example.com");
  REQUIRE(result->getUserType() == User::UserType::USER);
}

// 4. LOGIN / REGISTER / UPDATE_USER reject non-SUCCESS
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
  SECTION("UPDATE_USER ERROR") {
    const Packet packet = authPacket(Packet::PacketType::UPDATE_USER,
                                     static_cast<int>(RESPONSE_CODES::ERROR), payload);
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
  SECTION("UPDATE_USER malformed") {
    const Packet packet =
        authPacket(Packet::PacketType::UPDATE_USER, static_cast<int>(RESPONSE_CODES::SUCCESS),
                   "not-a-user");
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
}

// 6. every type other than LOGIN / REGISTER / UPDATE_USER returns nullopt
TEST_CASE("PacketHandler ignores every non-user packet type", "[packet_handler][edge]") {
  PacketHandler handler;
  const std::string payload = User("alice", "a@b.com", User::UserType::USER).serialize();
  int checked = 0;

  for (Packet::PacketType type : allPacketTypes()) {
    if (returnsUser(type)) {
      continue;
    }
    const Packet packet =
        authPacket(type, static_cast<int>(RESPONSE_CODES::SUCCESS), payload);
    INFO(Packet::packetTypeToString(type));
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
    ++checked;
  }

  REQUIRE(checked == static_cast<int>(allPacketTypes().size()) - 3);
}

// 7. SUCCESS with empty message fails
TEST_CASE("PacketHandler SUCCESS with empty message fails", "[packet_handler][edge]") {
  PacketHandler handler;
  for (Packet::PacketType type : {Packet::PacketType::LOGIN, Packet::PacketType::REGISTER,
                                  Packet::PacketType::UPDATE_USER}) {
    const Packet packet = authPacket(type, static_cast<int>(RESPONSE_CODES::SUCCESS), "");
    INFO(Packet::packetTypeToString(type));
    REQUIRE_FALSE(handler.handlePacket(packet).has_value());
  }
}

// 8. responseCode edge values
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
