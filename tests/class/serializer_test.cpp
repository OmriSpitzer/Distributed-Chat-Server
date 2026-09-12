/**
 * Serializer Class unit tests
 *
 * @brief Includes: round-trip all types, empty / unicode / special fields,
 * numeric extremes, serialize rejects invalid type and oversize payload,
 * deserialize rejects empty / truncated / extended frames, double round-trip.
 * @date 12-09-2026
 */

#include "utils/models/packet.h"
#include "utils/serializer.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <vector>

/**
 * 1. round-trip all packet types
 * 2. empty and whitespace fields
 * 3. unicode and special characters
 * 4. timestamp and responseCode extremes
 * 5. serialize rejects invalid type
 * 6. serialize rejects oversize payload
 * 7. deserialize rejects empty / truncated / extended frames
 * 8. double round-trip stability
 */

namespace {

// check if two packets are equal
void requirePacketsEqual(const Packet &expected, const Packet &actual) {
  REQUIRE(actual.type == expected.type);
  REQUIRE(actual.sender == expected.sender);
  REQUIRE(actual.receiver == expected.receiver);
  REQUIRE(actual.room == expected.room);
  REQUIRE(actual.message == expected.message);
  REQUIRE(actual.timestamp == expected.timestamp);
  REQUIRE(actual.responseCode == expected.responseCode);
}

// check if a packet can be serialized and deserialized
void requireRoundTrip(const Packet &original) {
  const std::string bytes = Serializer::serialize(original);
  REQUIRE_FALSE(bytes.empty());
  const auto restored = Serializer::deserialize(bytes);
  REQUIRE(restored.has_value());
  requirePacketsEqual(original, *restored);
}

// create a packet
Packet makePacket(Packet::PacketType type, const std::string &sender = "alice",
                  const std::string &receiver = "bob", const std::string &room = "general",
                  const std::string &message = "hello", std::uint64_t timestamp = 1,
                  int responseCode = 0) {
  Packet packet(sender, receiver, type, room, message, responseCode);
  packet.timestamp = timestamp;
  return packet;
}

} // namespace

// 1. round-trip all packet types
TEST_CASE("Serializer round-trip all packet types", "[serializer][roundtrip]") {
  SECTION("DEFAULT") { requireRoundTrip(makePacket(Packet::PacketType::DEFAULT)); }
  SECTION("LOGIN") {
    requireRoundTrip(makePacket(Packet::PacketType::LOGIN, "alice", "server", "", "secret"));
  }
  SECTION("LOGOUT") {
    requireRoundTrip(makePacket(Packet::PacketType::LOGOUT, "alice", "server", "", ""));
  }
  SECTION("MESSAGE") { requireRoundTrip(makePacket(Packet::PacketType::MESSAGE)); }
  SECTION("ROOM_JOIN") {
    requireRoundTrip(makePacket(Packet::PacketType::ROOM_JOIN, "alice", "server", "general", ""));
  }
  SECTION("ROOM_LEAVE") {
    requireRoundTrip(makePacket(Packet::PacketType::ROOM_LEAVE, "alice", "server", "general", ""));
  }
  SECTION("HEARTBEAT") {
    requireRoundTrip(makePacket(Packet::PacketType::HEARTBEAT, "heartbeat", "server", "", "ping"));
  }
  SECTION("REGISTER") {
    requireRoundTrip(
        makePacket(Packet::PacketType::REGISTER, "alice", "server", "", "secret", 10, 201));
  }
  SECTION("GOSSIP_HELLO") {
    requireRoundTrip(makePacket(Packet::PacketType::GOSSIP_HELLO, "node-a", "node-b", "", "hello"));
  }
  SECTION("GOSSIP_EVENT") {
    requireRoundTrip(
        makePacket(Packet::PacketType::GOSSIP_EVENT, "node-a", "node-b", "general", "event"));
  }
  SECTION("GOSSIP_DIGEST") {
    requireRoundTrip(
        makePacket(Packet::PacketType::GOSSIP_DIGEST, "node-a", "node-b", "", "digest"));
  }
  SECTION("GOSSIP_PULL") {
    requireRoundTrip(makePacket(Packet::PacketType::GOSSIP_PULL, "node-a", "node-b", "", "pull"));
  }
}

// 2. empty and whitespace fields
TEST_CASE("Serializer empty and whitespace fields", "[serializer][fields][edge]") {
  // all empty strings
  SECTION("all empty strings") {
    requireRoundTrip(makePacket(Packet::PacketType::MESSAGE, "", "", "", "", 0, 0));
  }

  // whitespace preserved
  SECTION("whitespace preserved") {
    requireRoundTrip(makePacket(Packet::PacketType::MESSAGE, "  alice  ", "  bob  ", "  room  ",
                                "  hi  ", 42, 7));
  }

  // default-constructed packet
  SECTION("default-constructed packet") {
    Packet packet;
    packet.timestamp = 0;
    packet.responseCode = 0;
    requireRoundTrip(packet);
  }

  // only sender set
  SECTION("only sender set") {
    Packet packet;
    packet.type = Packet::PacketType::LOGIN;
    packet.sender = "alice";
    packet.timestamp = 1;
    requireRoundTrip(packet);
  }

  // only message set
  SECTION("only message set") {
    Packet packet;
    packet.type = Packet::PacketType::MESSAGE;
    packet.message = "solo";
    packet.timestamp = 2;
    requireRoundTrip(packet);
  }
}

// 3. unicode and special characters
TEST_CASE("Serializer unicode and special characters", "[serializer][fields][edge]") {
  // unicode
  SECTION("unicode") {
    requireRoundTrip(
        makePacket(Packet::PacketType::MESSAGE, "עֹמְרִי", "ボブ", "חדר", "שלום 👋", 999, 1));
  }

  // embedded null bytes
  SECTION("embedded null bytes") {
    Packet packet;
    packet.type = Packet::PacketType::MESSAGE;
    packet.sender = std::string("a\0b", 3);
    packet.receiver = std::string("c\0d", 3);
    packet.room = std::string("e\0f", 3);
    packet.message = std::string("g\0h\0i", 5);
    packet.timestamp = 12345;
    packet.responseCode = 0;
    requireRoundTrip(packet);
  }

  // newlines and tabs
  SECTION("newlines and tabs") {
    requireRoundTrip(makePacket(Packet::PacketType::MESSAGE, "alice", "bob", "general",
                                "line1\nline2\r\nline3\tend", 10, 0));
  }

  // pipes and braces
  SECTION("pipes and braces") {
    requireRoundTrip(
        makePacket(Packet::PacketType::MESSAGE, "a|b", "{bob}", "r(oom)", "msg|with|pipes", 11, 0));
  }

  // long but under limit message
  SECTION("long but under limit message") {
    requireRoundTrip(makePacket(Packet::PacketType::MESSAGE, "alice", "bob", "general",
                                std::string(64 * 1024, 'm'), 12, 0));
  }
}

// 4. timestamp and responseCode extremes
TEST_CASE("Serializer timestamp and responseCode extremes", "[serializer][numeric][edge]") {
  // timestamp zero
  SECTION("timestamp zero") {
    requireRoundTrip(makePacket(Packet::PacketType::MESSAGE, "a", "b", "", "", 0, 0));
  }

  // timestamp max uint64
  SECTION("timestamp max uint64") {
    requireRoundTrip(makePacket(Packet::PacketType::MESSAGE, "a", "b", "", "", UINT64_MAX, 0));
  }

  // responseCode negative
  SECTION("responseCode negative") {
    requireRoundTrip(makePacket(Packet::PacketType::LOGIN, "a", "s", "", "", 1, -1));
  }

  // responseCode INT_MIN
  SECTION("responseCode INT_MIN") {
    requireRoundTrip(makePacket(Packet::PacketType::LOGIN, "a", "s", "", "", 2, INT32_MIN));
  }

  // responseCode INT_MAX
  SECTION("responseCode INT_MAX") {
    requireRoundTrip(makePacket(Packet::PacketType::LOGIN, "a", "s", "", "", 3, INT32_MAX));
  }

  // responseCode typical codes
  SECTION("responseCode typical codes") {
    for (int code : {0, 200, 400, 401, 403, 404, 500}) {
      requireRoundTrip(makePacket(Packet::PacketType::REGISTER, "a", "s", "", "pw", 50, code));
    }
  }
}

// 5. serialize rejects invalid type
TEST_CASE("Serializer serialize rejects invalid type", "[serializer][serialize][edge]") {
  // out of range type byte
  SECTION("out of range type byte") {
    Packet packet = makePacket(Packet::PacketType::MESSAGE);
    packet.type = static_cast<Packet::PacketType>(255);
    REQUIRE(Serializer::serialize(packet).empty());
  }

  // first unused enum value after GOSSIP_PULL
  SECTION("first unused enum value after GOSSIP_PULL") {
    Packet packet = makePacket(Packet::PacketType::MESSAGE);
    packet.type =
        static_cast<Packet::PacketType>(static_cast<int>(Packet::PacketType::GOSSIP_PULL) + 1);
    REQUIRE(Serializer::serialize(packet).empty());
  }
}

// 6. serialize rejects oversize payload
TEST_CASE("Serializer serialize rejects oversize payload", "[serializer][serialize][edge]") {
  // Fixed overhead: 1 type + 8 ts + 4 code + 4*4 length prefixes = 29 bytes
  constexpr std::size_t kFixedOverhead = 29;

  // payload exactly at MAX_PAYLOAD_BYTES succeeds
  SECTION("payload exactly at MAX_PAYLOAD_BYTES succeeds") {
    Packet packet =
        makePacket(Packet::PacketType::MESSAGE, "", "", "",
                   std::string(Serializer::MAX_PAYLOAD_BYTES - kFixedOverhead, 'x'), 1, 0);
    const std::string framed = Serializer::serialize(packet);
    REQUIRE_FALSE(framed.empty());
    const auto restored = Serializer::deserialize(framed);
    REQUIRE(restored.has_value());
    REQUIRE(restored->message.size() == Serializer::MAX_PAYLOAD_BYTES - kFixedOverhead);
  }

  // payload one byte over MAX_PAYLOAD_BYTES fails
  SECTION("payload one byte over MAX_PAYLOAD_BYTES fails") {
    Packet packet =
        makePacket(Packet::PacketType::MESSAGE, "", "", "",
                   std::string(Serializer::MAX_PAYLOAD_BYTES - kFixedOverhead + 1, 'y'), 1, 0);
    REQUIRE(Serializer::serialize(packet).empty());
  }
}

// 7. deserialize rejects empty / truncated / extended frames
TEST_CASE("Serializer deserialize rejects bad frames", "[serializer][deserialize][edge]") {
  const Packet sample = makePacket(Packet::PacketType::MESSAGE);
  const std::string valid = Serializer::serialize(sample);

  // valid frame
  REQUIRE_FALSE(valid.empty());
  SECTION("empty buffer") { REQUIRE_FALSE(Serializer::deserialize("").has_value()); }
  SECTION("one byte") { REQUIRE_FALSE(Serializer::deserialize(std::string(1, '\0')).has_value()); }
  SECTION("three bytes") {
    REQUIRE_FALSE(Serializer::deserialize(std::string("\x00\x00\x00", 3)).has_value());
  }

  // truncated valid frame
  SECTION("truncated valid frame") {
    REQUIRE_FALSE(Serializer::deserialize(valid.substr(0, valid.size() - 1)).has_value());
  }

  // extra trailing byte
  SECTION("extra trailing byte") {
    REQUIRE_FALSE(Serializer::deserialize(valid + "x").has_value());
  }

  // corrupted type byte
  SECTION("corrupted type byte") {
    std::string bad = valid;
    bad[4] = static_cast<char>(200);
    REQUIRE_FALSE(Serializer::deserialize(bad).has_value());
  }
}

// 8. double round-trip stability
TEST_CASE("Serializer double round-trip is stable", "[serializer][roundtrip][edge]") {
  const std::vector<Packet> samples = {
      makePacket(Packet::PacketType::LOGIN, "alice", "server", "", "secret", 1, 0),
      makePacket(Packet::PacketType::MESSAGE, "alice", "bob", "general", "hello", 2, 404),
      makePacket(Packet::PacketType::MESSAGE, "עֹמְרִי", "ボブ", "חדר", "שלום", 3, -5),
      makePacket(Packet::PacketType::MESSAGE, "", "", "", "", 4, INT32_MAX),
      makePacket(Packet::PacketType::GOSSIP_EVENT, "node-a", "node-b", "general", "event",
                 UINT64_MAX, INT32_MIN),
  };

  for (const Packet &original : samples) {
    const std::string once = Serializer::serialize(original);
    const auto mid = Serializer::deserialize(once);
    REQUIRE(mid.has_value());
    const std::string twice = Serializer::serialize(*mid);
    REQUIRE(twice == once);
    const auto again = Serializer::deserialize(twice);
    REQUIRE(again.has_value());
    requirePacketsEqual(original, *again);
  }
}
