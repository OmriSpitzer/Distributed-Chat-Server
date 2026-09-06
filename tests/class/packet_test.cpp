/**
 * Packet Class unit tests
 */

#include "utils/models/packet.h"

#include <catch2/catch_test_macros.hpp>

#include <ctime>
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

  SECTION("HEARTBEAT") {
    const Packet packet("heartbeat", "server", Packet::PacketType::HEARTBEAT, "", "ping");
    REQUIRE(packet.type == Packet::PacketType::HEARTBEAT);
    REQUIRE(packet.sender == "heartbeat");
    REQUIRE(packet.receiver == "server");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message == "ping");
  }

  SECTION("REGISTER") {
    const Packet packet("alice", "server", Packet::PacketType::REGISTER, "", "secret");
    REQUIRE(packet.type == Packet::PacketType::REGISTER);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "server");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message == "secret");
  }

  SECTION("default type when omitted") {
    const Packet packet("alice", "bob");
    REQUIRE(packet.type == Packet::PacketType::DEFAULT);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "bob");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message.empty());
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

  SECTION("copy is independent of original") {
    Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello");
    Packet copied = original.copy();
    original.sender = "changed";
    original.message = "mutated";
    REQUIRE(copied.sender == "alice");
    REQUIRE(copied.message == "hello");
  }
}
