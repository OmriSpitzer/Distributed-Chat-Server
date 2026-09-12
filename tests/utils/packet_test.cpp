/**
 * Packet Class unit tests
 *
 * @brief Includes: default ctor, all types, default args, field edges, responseCode,
 * timestamp, copy/move/assign, packetTypeToString, stringToPacketType, round-trip,
 * public-field mutation.
 * @date 12-09-2026
 */

#include "utils/models/packet.h"

#include <catch2/catch_test_macros.hpp>

#include <climits>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * 1. default constructor
 * 2. constructor stores sender, receiver, type, room, message, responseCode
 * 3. constructor default arguments
 * 4. sender / receiver / room / message edge cases
 * 5. responseCode edge cases
 * 6. timestamp
 * 7. copy, move, and assignment
 * 8. packetTypeToString
 * 9. stringToPacketType
 * 10. type conversion round-trip
 * 11. public field mutation
 */

// 1. default constructor
TEST_CASE("Default Packet has DEFAULT type and empty fields", "[packet][ctor]") {
  const Packet packet;

  REQUIRE(packet.type == Packet::PacketType::DEFAULT);
  REQUIRE(packet.sender.empty());
  REQUIRE(packet.receiver.empty());
  REQUIRE(packet.room.empty());
  REQUIRE(packet.message.empty());
  REQUIRE(packet.timestamp == 0);
  REQUIRE(packet.responseCode == 0);
}

// 2. constructor stores sender, receiver, type, room, message, responseCode
TEST_CASE("Packet constructor stores all fields", "[packet][ctor]") {
  SECTION("LOGIN") {
    const Packet packet("alice", "server", Packet::PacketType::LOGIN, "", "secret", 0);
    REQUIRE(packet.type == Packet::PacketType::LOGIN);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "server");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message == "secret");
    REQUIRE(packet.responseCode == 0);
  }

  SECTION("LOGOUT") {
    const Packet packet("alice", "server", Packet::PacketType::LOGOUT);
    REQUIRE(packet.type == Packet::PacketType::LOGOUT);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "server");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message.empty());
    REQUIRE(packet.responseCode == 0);
  }

  SECTION("MESSAGE") {
    const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello", 200);
    REQUIRE(packet.type == Packet::PacketType::MESSAGE);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "bob");
    REQUIRE(packet.room == "general");
    REQUIRE(packet.message == "hello");
    REQUIRE(packet.responseCode == 200);
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

  SECTION("DEFAULT explicit") {
    const Packet packet("alice", "bob", Packet::PacketType::DEFAULT, "lobby", "noop", 1);
    REQUIRE(packet.type == Packet::PacketType::DEFAULT);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "bob");
    REQUIRE(packet.room == "lobby");
    REQUIRE(packet.message == "noop");
    REQUIRE(packet.responseCode == 1);
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
    const Packet packet("alice", "server", Packet::PacketType::REGISTER, "", "secret", 201);
    REQUIRE(packet.type == Packet::PacketType::REGISTER);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "server");
    REQUIRE(packet.message == "secret");
    REQUIRE(packet.responseCode == 201);
  }

  SECTION("GOSSIP_HELLO") {
    const Packet packet("node-a", "node-b", Packet::PacketType::GOSSIP_HELLO, "", "hello");
    REQUIRE(packet.type == Packet::PacketType::GOSSIP_HELLO);
    REQUIRE(packet.sender == "node-a");
    REQUIRE(packet.receiver == "node-b");
    REQUIRE(packet.message == "hello");
  }

  SECTION("GOSSIP_EVENT") {
    const Packet packet("node-a", "*", Packet::PacketType::GOSSIP_EVENT, "general", "event");
    REQUIRE(packet.type == Packet::PacketType::GOSSIP_EVENT);
    REQUIRE(packet.receiver == "*");
    REQUIRE(packet.room == "general");
    REQUIRE(packet.message == "event");
  }

  SECTION("GOSSIP_DIGEST") {
    const Packet packet("node-a", "node-b", Packet::PacketType::GOSSIP_DIGEST, "", "digest");
    REQUIRE(packet.type == Packet::PacketType::GOSSIP_DIGEST);
    REQUIRE(packet.message == "digest");
  }

  SECTION("GOSSIP_PULL") {
    const Packet packet("node-a", "node-b", Packet::PacketType::GOSSIP_PULL, "", "pull");
    REQUIRE(packet.type == Packet::PacketType::GOSSIP_PULL);
    REQUIRE(packet.message == "pull");
  }
}

// 3. constructor default arguments
TEST_CASE("Packet constructor default arguments", "[packet][ctor][defaults]") {
  SECTION("type omitted is DEFAULT") {
    const Packet packet("alice", "bob");
    REQUIRE(packet.type == Packet::PacketType::DEFAULT);
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "bob");
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message.empty());
    REQUIRE(packet.responseCode == 0);
  }

  SECTION("room omitted is empty") {
    const Packet packet("alice", "server", Packet::PacketType::LOGOUT);
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message.empty());
    REQUIRE(packet.responseCode == 0);
  }

  SECTION("message omitted is empty") {
    const Packet packet("alice", "server", Packet::PacketType::ROOM_JOIN, "general");
    REQUIRE(packet.room == "general");
    REQUIRE(packet.message.empty());
    REQUIRE(packet.responseCode == 0);
  }

  SECTION("responseCode omitted is 0") {
    const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello");
    REQUIRE(packet.responseCode == 0);
  }
}

// 4. sender / receiver / room / message edge cases
TEST_CASE("Packet field edge cases", "[packet][ctor][edge]") {
  SECTION("all empty strings") {
    const Packet packet("", "", Packet::PacketType::MESSAGE, "", "", 0);
    REQUIRE(packet.sender.empty());
    REQUIRE(packet.receiver.empty());
    REQUIRE(packet.room.empty());
    REQUIRE(packet.message.empty());
  }

  SECTION("whitespace only") {
    const Packet packet("   \t  ", "  ", Packet::PacketType::MESSAGE, " \n ", "   \t  ");
    REQUIRE(packet.sender == "   \t  ");
    REQUIRE(packet.receiver == "  ");
    REQUIRE(packet.room == " \n ");
    REQUIRE(packet.message == "   \t  ");
  }

  SECTION("leading and trailing spaces") {
    const Packet packet("  alice  ", "  bob  ", Packet::PacketType::MESSAGE, "  room  ",
                        "  hello  ");
    REQUIRE(packet.sender == "  alice  ");
    REQUIRE(packet.receiver == "  bob  ");
    REQUIRE(packet.room == "  room  ");
    REQUIRE(packet.message == "  hello  ");
  }

  SECTION("single character") {
    const Packet packet("a", "b", Packet::PacketType::MESSAGE, "r", "x");
    REQUIRE(packet.sender == "a");
    REQUIRE(packet.receiver == "b");
    REQUIRE(packet.room == "r");
    REQUIRE(packet.message == "x");
  }

  SECTION("unicode") {
    const Packet packet("עֹמְרִי", "ボブ", Packet::PacketType::MESSAGE, "חדר", "שלום 👋");
    REQUIRE(packet.sender == "עֹמְרִי");
    REQUIRE(packet.receiver == "ボブ");
    REQUIRE(packet.room == "חדר");
    REQUIRE(packet.message == "שלום 👋");
  }

  SECTION("special characters") {
    const std::string special = "a|b{c}\"d'\\e<>&;%@#";
    const Packet packet(special, special, Packet::PacketType::MESSAGE, special, special);
    REQUIRE(packet.sender == special);
    REQUIRE(packet.receiver == special);
    REQUIRE(packet.room == special);
    REQUIRE(packet.message == special);
  }

  SECTION("pipes and serializer delimiters") {
    const Packet packet("a|b", "{bob}", Packet::PacketType::MESSAGE, "r(oom)", "msg|with|pipes");
    REQUIRE(packet.sender == "a|b");
    REQUIRE(packet.receiver == "{bob}");
    REQUIRE(packet.room == "r(oom)");
    REQUIRE(packet.message == "msg|with|pipes");
  }

  SECTION("multiline message") {
    const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "line1\nline2");
    REQUIRE(packet.message == "line1\nline2");
  }

  SECTION("embedded null in message") {
    const char raw[] = {'h', 'i', '\0', '!', '\0'};
    const std::string_view view(raw, sizeof(raw));
    const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "", view);
    REQUIRE(packet.message.size() == sizeof(raw));
    REQUIRE(packet.message == std::string(view));
  }

  SECTION("very long fields") {
    const std::string longField(4096, 'x');
    const Packet packet(longField, longField, Packet::PacketType::MESSAGE, longField, longField);
    REQUIRE(packet.sender.size() == 4096);
    REQUIRE(packet.receiver.size() == 4096);
    REQUIRE(packet.room.size() == 4096);
    REQUIRE(packet.message.size() == 4096);
    REQUIRE(packet.sender == longField);
    REQUIRE(packet.message == longField);
  }

  SECTION("source string mutation does not change packet") {
    std::string sender = "alice";
    std::string message = "hello";
    const Packet packet(sender, "bob", Packet::PacketType::MESSAGE, "general", message);
    sender = "changed";
    message = "mutated";
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.message == "hello");
  }

  SECTION("broadcast receiver star") {
    const Packet packet("node-a", "*", Packet::PacketType::GOSSIP_EVENT, "", "payload");
    REQUIRE(packet.receiver == "*");
  }
}

// 5. responseCode edge cases
TEST_CASE("Packet responseCode edge cases", "[packet][responseCode][edge]") {
  SECTION("zero") {
    const Packet packet("a", "b", Packet::PacketType::MESSAGE, "", "", 0);
    REQUIRE(packet.responseCode == 0);
  }

  SECTION("negative") {
    const Packet packet("a", "s", Packet::PacketType::LOGIN, "", "", -1);
    REQUIRE(packet.responseCode == -1);
  }

  SECTION("INT_MIN") {
    const Packet packet("a", "s", Packet::PacketType::LOGIN, "", "", INT_MIN);
    REQUIRE(packet.responseCode == INT_MIN);
  }

  SECTION("INT_MAX") {
    const Packet packet("a", "s", Packet::PacketType::LOGIN, "", "", INT_MAX);
    REQUIRE(packet.responseCode == INT_MAX);
  }

  SECTION("typical codes") {
    REQUIRE(Packet("a", "s", Packet::PacketType::REGISTER, "", "pw", 200).responseCode == 200);
    REQUIRE(Packet("a", "s", Packet::PacketType::REGISTER, "", "pw", 201).responseCode == 201);
    REQUIRE(Packet("a", "s", Packet::PacketType::REGISTER, "", "pw", 400).responseCode == 400);
    REQUIRE(Packet("a", "s", Packet::PacketType::REGISTER, "", "pw", 401).responseCode == 401);
    REQUIRE(Packet("a", "s", Packet::PacketType::REGISTER, "", "pw", 404).responseCode == 404);
    REQUIRE(Packet("a", "s", Packet::PacketType::REGISTER, "", "pw", 500).responseCode == 500);
  }
}

// 6. timestamp
TEST_CASE("Packet timestamp is set near construction time", "[packet][timestamp]") {
  SECTION("parameterized constructor stamps now") {
    const auto before = static_cast<uint64_t>(std::time(nullptr));
    const Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "ping");
    const auto after = static_cast<uint64_t>(std::time(nullptr));

    REQUIRE(packet.timestamp >= before);
    REQUIRE(packet.timestamp <= after);
  }

  SECTION("default constructor stays zero") {
    const Packet packet;
    REQUIRE(packet.timestamp == 0);
  }

  SECTION("public field accepts extremes") {
    Packet packet("a", "b", Packet::PacketType::MESSAGE);
    packet.timestamp = 0;
    REQUIRE(packet.timestamp == 0);
    packet.timestamp = UINT64_MAX;
    REQUIRE(packet.timestamp == UINT64_MAX);
  }
}

// 7. copy, move, and assignment
TEST_CASE("Packet copy preserves all fields", "[packet][copy]") {
  SECTION("filled message packet") {
    const Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello", 404);
    const Packet copied = original;

    REQUIRE(copied.type == original.type);
    REQUIRE(copied.sender == original.sender);
    REQUIRE(copied.receiver == original.receiver);
    REQUIRE(copied.room == original.room);
    REQUIRE(copied.message == original.message);
    REQUIRE(copied.timestamp == original.timestamp);
    REQUIRE(copied.responseCode == original.responseCode);
  }

  SECTION("empty fields") {
    const Packet original("", "", Packet::PacketType::DEFAULT, "", "", 0);
    const Packet copied = original;

    REQUIRE(copied.type == Packet::PacketType::DEFAULT);
    REQUIRE(copied.sender.empty());
    REQUIRE(copied.receiver.empty());
    REQUIRE(copied.room.empty());
    REQUIRE(copied.message.empty());
    REQUIRE(copied.timestamp == original.timestamp);
    REQUIRE(copied.responseCode == 0);
  }

  SECTION("gossip packet") {
    const Packet original("node-a", "*", Packet::PacketType::GOSSIP_EVENT, "lobby", "payload", 0);
    const Packet copied = original;
    REQUIRE(copied.type == Packet::PacketType::GOSSIP_EVENT);
    REQUIRE(copied.sender == "node-a");
    REQUIRE(copied.receiver == "*");
    REQUIRE(copied.room == "lobby");
    REQUIRE(copied.message == "payload");
  }

  SECTION("timestamp is preserved after mutation") {
    Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello");
    original.timestamp = 1725450000;
    const Packet copied = original;
    REQUIRE(copied.timestamp == 1725450000);
    REQUIRE(copied.timestamp == original.timestamp);
  }

  SECTION("copy is independent of original") {
    Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello", 200);
    Packet copied = original;
    original.type = Packet::PacketType::LOGOUT;
    original.sender = "changed";
    original.receiver = "other";
    original.room = "elsewhere";
    original.message = "mutated";
    original.timestamp = 1;
    original.responseCode = 500;

    REQUIRE(copied.type == Packet::PacketType::MESSAGE);
    REQUIRE(copied.sender == "alice");
    REQUIRE(copied.receiver == "bob");
    REQUIRE(copied.room == "general");
    REQUIRE(copied.message == "hello");
    REQUIRE(copied.responseCode == 200);
    REQUIRE(copied.timestamp != 1);
  }

  SECTION("copy assignment") {
    const Packet original("alice", "bob", Packet::PacketType::LOGIN, "", "secret", 401);
    Packet assigned;
    assigned = original;
    REQUIRE(assigned.type == Packet::PacketType::LOGIN);
    REQUIRE(assigned.sender == "alice");
    REQUIRE(assigned.receiver == "bob");
    REQUIRE(assigned.message == "secret");
    REQUIRE(assigned.responseCode == 401);
    REQUIRE(assigned.timestamp == original.timestamp);
  }

  SECTION("self assignment") {
    Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello", 10);
    packet = packet;
    REQUIRE(packet.sender == "alice");
    REQUIRE(packet.receiver == "bob");
    REQUIRE(packet.room == "general");
    REQUIRE(packet.message == "hello");
    REQUIRE(packet.responseCode == 10);
  }

  SECTION("move constructor") {
    Packet original("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello", 200);
    const uint64_t ts = original.timestamp;
    Packet moved = std::move(original);
    REQUIRE(moved.type == Packet::PacketType::MESSAGE);
    REQUIRE(moved.sender == "alice");
    REQUIRE(moved.receiver == "bob");
    REQUIRE(moved.room == "general");
    REQUIRE(moved.message == "hello");
    REQUIRE(moved.timestamp == ts);
    REQUIRE(moved.responseCode == 200);
  }

  SECTION("move assignment") {
    Packet original("alice", "server", Packet::PacketType::REGISTER, "", "pw", 201);
    Packet dest;
    dest = std::move(original);
    REQUIRE(dest.type == Packet::PacketType::REGISTER);
    REQUIRE(dest.sender == "alice");
    REQUIRE(dest.message == "pw");
    REQUIRE(dest.responseCode == 201);
  }
}

// 8. packetTypeToString
TEST_CASE("Packet packetTypeToString", "[packet][packetTypeToString]") {
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::LOGIN) == "LOGIN");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::LOGOUT) == "LOGOUT");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::MESSAGE) == "MESSAGE");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::ROOM_JOIN) == "ROOM_JOIN");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::ROOM_LEAVE) == "ROOM_LEAVE");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::DEFAULT) == "DEFAULT");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::HEARTBEAT) == "HEARTBEAT");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::REGISTER) == "REGISTER");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::GOSSIP_HELLO) == "GOSSIP_HELLO");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::GOSSIP_EVENT) == "GOSSIP_EVENT");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::GOSSIP_DIGEST) == "GOSSIP_DIGEST");
  REQUIRE(Packet::packetTypeToString(Packet::PacketType::GOSSIP_PULL) == "GOSSIP_PULL");

  SECTION("invalid enum value defaults to DEFAULT") {
    const auto bogus = static_cast<Packet::PacketType>(999);
    REQUIRE(Packet::packetTypeToString(bogus) == "DEFAULT");
  }

  SECTION("negative enum value defaults to DEFAULT") {
    const auto bogus = static_cast<Packet::PacketType>(-1);
    REQUIRE(Packet::packetTypeToString(bogus) == "DEFAULT");
  }
}

// 9. stringToPacketType
TEST_CASE("Packet stringToPacketType", "[packet][stringToPacketType]") {
  REQUIRE(Packet::stringToPacketType("LOGIN") == Packet::PacketType::LOGIN);
  REQUIRE(Packet::stringToPacketType("LOGOUT") == Packet::PacketType::LOGOUT);
  REQUIRE(Packet::stringToPacketType("MESSAGE") == Packet::PacketType::MESSAGE);
  REQUIRE(Packet::stringToPacketType("ROOM_JOIN") == Packet::PacketType::ROOM_JOIN);
  REQUIRE(Packet::stringToPacketType("ROOM_LEAVE") == Packet::PacketType::ROOM_LEAVE);
  REQUIRE(Packet::stringToPacketType("DEFAULT") == Packet::PacketType::DEFAULT);
  REQUIRE(Packet::stringToPacketType("HEARTBEAT") == Packet::PacketType::HEARTBEAT);
  REQUIRE(Packet::stringToPacketType("REGISTER") == Packet::PacketType::REGISTER);
  REQUIRE(Packet::stringToPacketType("GOSSIP_HELLO") == Packet::PacketType::GOSSIP_HELLO);
  REQUIRE(Packet::stringToPacketType("GOSSIP_EVENT") == Packet::PacketType::GOSSIP_EVENT);
  REQUIRE(Packet::stringToPacketType("GOSSIP_DIGEST") == Packet::PacketType::GOSSIP_DIGEST);
  REQUIRE(Packet::stringToPacketType("GOSSIP_PULL") == Packet::PacketType::GOSSIP_PULL);

  SECTION("unknown / edge strings default to DEFAULT") {
    REQUIRE(Packet::stringToPacketType("") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("login") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("Login") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("MESSAGE ") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType(" MESSAGE") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("UNKNOWN") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("heartbeat") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("GOSSIP EVENT") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("GOSSIP") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("LOG") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("DEFAULT ") == Packet::PacketType::DEFAULT);
    REQUIRE(Packet::stringToPacketType("null") == Packet::PacketType::DEFAULT);
  }

  SECTION("string_view into owned string") {
    const std::string name = "ROOM_JOIN";
    REQUIRE(Packet::stringToPacketType(name) == Packet::PacketType::ROOM_JOIN);
  }
}

// 10. type conversion round-trip
TEST_CASE("Packet type conversion round-trip", "[packet][type-roundtrip]") {
  const std::vector<Packet::PacketType> types = {
      Packet::PacketType::LOGIN,         Packet::PacketType::LOGOUT,
      Packet::PacketType::MESSAGE,       Packet::PacketType::ROOM_JOIN,
      Packet::PacketType::ROOM_LEAVE,    Packet::PacketType::DEFAULT,
      Packet::PacketType::HEARTBEAT,     Packet::PacketType::REGISTER,
      Packet::PacketType::GOSSIP_HELLO,  Packet::PacketType::GOSSIP_EVENT,
      Packet::PacketType::GOSSIP_DIGEST, Packet::PacketType::GOSSIP_PULL};

  for (const auto type : types) {
    REQUIRE(Packet::stringToPacketType(Packet::packetTypeToString(type)) == type);
  }
}

// 11. public field mutation
TEST_CASE("Packet public fields are independently mutable", "[packet][fields]") {
  Packet packet("alice", "bob", Packet::PacketType::MESSAGE, "general", "hello", 200);

  packet.type = Packet::PacketType::HEARTBEAT;
  REQUIRE(packet.type == Packet::PacketType::HEARTBEAT);
  REQUIRE(packet.sender == "alice");
  REQUIRE(packet.message == "hello");

  packet.sender = "carol";
  packet.receiver = "dave";
  packet.room = "other";
  packet.message = "pong";
  packet.responseCode = 418;
  packet.timestamp = 42;

  REQUIRE(packet.sender == "carol");
  REQUIRE(packet.receiver == "dave");
  REQUIRE(packet.room == "other");
  REQUIRE(packet.message == "pong");
  REQUIRE(packet.responseCode == 418);
  REQUIRE(packet.timestamp == 42);
  REQUIRE(packet.type == Packet::PacketType::HEARTBEAT);

  packet.sender.clear();
  packet.message.clear();
  REQUIRE(packet.sender.empty());
  REQUIRE(packet.message.empty());
  REQUIRE(packet.receiver == "dave");
}
