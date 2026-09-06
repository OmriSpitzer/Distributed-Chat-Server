/**
 * Packet Class unit tests
 */

#include "utils/models/packet.h"

#include <catch2/catch_test_macros.hpp>

#include <ctime>
#include <stdexcept>
#include <string>

TEST_CASE("Default Packet has DEFAULT type and empty fields", "[packet][ctor]") {
  const Packet packet;

  REQUIRE(packet.type == Packet::PacketType::DEFAULT);
  REQUIRE(packet.sender.empty());
  REQUIRE(packet.receiver.empty());
  REQUIRE(packet.room.empty());
  REQUIRE(packet.message.empty());
  REQUIRE(packet.timestamp == 0);
}

TEST_CASE("Packet stores sender, receiver, type, room, and message", "[packet][ctor]") {
  SECTION("LOGIN") {
    const Packet packet("alice", "server", Packet::PacketType::LOGIN, "", "secret");
    REQUIRE(packet.type == Packet::PacketType::LOGIN);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "server");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message == "secret");
  }

  SECTION("LOGOUT") {
    const Packet packet("alice", "server", Packet::PacketType::LOGOUT);
    REQUIRE(packet.type == Packet::PacketType::LOGOUT);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "server");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message.empty());
  }

  SECTION("MESSAGE") {
    const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello");
    REQUIRE(packet.type == Packet::PacketType::MESSAGE);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "bob");
    REQUIRE(packet.room == "general");
    REQUIRE(packet.message == "hello");
  }

  SECTION("ROOM_JOIN") {
    const Packet packet("alice", "server", Packet::PacketType::ROOM_JOIN, "general");
    REQUIRE(packet.type == Packet::PacketType::ROOM_JOIN);
    REQUIRE(packet.room == "general");
  }

  SECTION("ROOM_LEAVE") {
    const Packet packet("alice", "server", Packet::PacketType::ROOM_LEAVE, "general");
    REQUIRE(packet.type == Packet::PacketType::ROOM_LEAVE);
    REQUIRE(packet.room == "general");
  }

  SECTION("empty fields") {
    const Packet packet("", "", Packet::PacketType::MESSAGE, "", "");
    REQUIRE(packet.sender.empty());
    REQUIRE(packet.receiver.empty());
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message.empty());
  }

  SECTION("multiline message") {
    const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "line1\nline2");
    REQUIRE(packet.message == "line1\nline2");
  }
}

TEST_CASE("Packet timestamp is set near construction time", "[packet][timestamp]") {
  const auto before = static_cast<uint64_t>(std::time(nullptr));
  const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "ping");
  const auto after = static_cast<uint64_t>(std::time(nullptr));

  REQUIRE(packet.timestamp >= before);
  REQUIRE(packet.timestamp <= after);
}

TEST_CASE("Packet copy preserves type and text fields", "[packet][copy]") {
  SECTION("filled message packet") {
    const Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello");
    const Packet copied = original.copy();

    REQUIRE(copied.type == original.type);
    REQUIRE(copied.sender == original.sender);
    REQUIRE(copied.receiver == original.receiver);
    REQUIRE(copied.room == original.room);
    REQUIRE(copied.message == original.message);
  }

  SECTION("empty fields") {
    const Packet original("", "", Packet::PacketType::DEFAULT, "", "");
    const Packet copied = original.copy();

    REQUIRE(copied.type == Packet::PacketType::DEFAULT);
    REQUIRE(copied.sender.empty());
    REQUIRE(copied.receiver.empty());
    REQUIRE(copied.room.empty());
    REQUIRE(copied.message.empty());
  }

  SECTION("timestamp is preserved") {
    Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello");
    original.timestamp = 1725450000;
    const Packet copied = original.copy();
    REQUIRE(copied.timestamp == original.timestamp);
  }
}

TEST_CASE("Packet serialize/deserialize round-trips fields", "[packet][serialize]") {
  SECTION("LOGIN with empty room") {
    Packet original("alice", "server", Packet::PacketType::LOGIN, "", "secret");
    const Packet restored = Packet::deserialize(original.serialize());

    REQUIRE(restored.type == original.type);
    REQUIRE(restored.sender == original.sender);
    REQUIRE(restored.receiver == original.receiver);
    REQUIRE(restored.room.empty());
    REQUIRE(restored.message == original.message);
    REQUIRE(restored.timestamp == original.timestamp);
  }

  SECTION("REGISTER") {
    Packet original("alice", "server", Packet::PacketType::REGISTER, "", "secret");
    const Packet restored = Packet::deserialize(original.serialize());

    REQUIRE(restored.type == Packet::PacketType::REGISTER);
    REQUIRE(restored.sender == "alice");
    REQUIRE(restored.message == "secret");
  }

  SECTION("MESSAGE with pipes, backslashes, and brackets") {
    Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "see [this]|and\\that");
    const Packet restored = Packet::deserialize(original.serialize());

    REQUIRE(restored.type == Packet::PacketType::MESSAGE);
    REQUIRE(restored.room == "general");
    REQUIRE(restored.message == "see [this]|and\\that");
    REQUIRE(restored.timestamp == original.timestamp);
  }

  SECTION("multiline message") {
    Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "line1\nline2");
    const Packet restored = Packet::deserialize(original.serialize());
    REQUIRE(restored.message == "line1\nline2");
  }
}

TEST_CASE("Packet deserialize rejects invalid payloads", "[packet][deserialize]") {
  SECTION("empty payload") { REQUIRE_THROWS_AS(Packet::deserialize(""), std::invalid_argument); }

  SECTION("wrong field count") {
    REQUIRE_THROWS_AS(Packet::deserialize("LOGIN|alice|server"), std::invalid_argument);
  }

  SECTION("unknown type") {
    REQUIRE_THROWS_AS(Packet::deserialize("NOPE|a|b|c|d|1"), std::invalid_argument);
  }

  SECTION("invalid timestamp") {
    REQUIRE_THROWS_AS(Packet::deserialize("LOGIN|a|b|c|d|not-a-number"), std::invalid_argument);
  }

  SECTION("dangling escape") {
    REQUIRE_THROWS_AS(Packet::deserialize("LOGIN|a|b|c|d\\"), std::invalid_argument);
  }
}
