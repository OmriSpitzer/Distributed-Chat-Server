/**
 * GossipPayload unit tests
 *
 * @brief Includes: round-trip, empty / unicode / pipes / null bytes,
 * wire size, decode rejects empty / truncated / trailing junk / oversize length,
 * eventId from full and partial payloads, legacy pipe format rejected.
 * @date 13-09-2026
 */

#include "utils/gossip_payload.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <string_view>

/**
 * 1. round-trip typical event fields
 * 2. empty fields
 * 3. pipes, braces, whitespace
 * 4. unicode and embedded nulls
 * 5. large content
 * 6. wire size matches header + bodies
 * 7. decode rejects empty / short / truncated
 * 8. decode rejects trailing junk / oversize length claim
 * 9. eventId from full and partial payloads
 * 10. legacy pipe-delimited payload rejected
 * 11. double round-trip stability
 */

namespace {

void requireFieldsEqual(const gossip_payload::Fields &expected,
                        const gossip_payload::Fields &actual) {
  REQUIRE(actual.type == expected.type);
  REQUIRE(actual.eventId == expected.eventId);
  REQUIRE(actual.username == expected.username);
  REQUIRE(actual.content == expected.content);
  REQUIRE(actual.field5 == expected.field5);
}

void requireRoundTrip(std::string_view type, std::string_view eventId, std::string_view username,
                      std::string_view content, std::string_view field5) {
  const std::string bytes = gossip_payload::encode(type, eventId, username, content, field5);
  const auto decoded = gossip_payload::decode(bytes);
  REQUIRE(decoded.has_value());
  requireFieldsEqual(gossip_payload::Fields{std::string(type), std::string(eventId),
                                            std::string(username), std::string(content),
                                            std::string(field5)},
                     *decoded);
  REQUIRE(gossip_payload::eventId(bytes) == eventId);
}

std::string appendU32BE(std::uint32_t value) {
  std::string out;
  out.push_back(static_cast<char>((value >> 24) & 0xFF));
  out.push_back(static_cast<char>((value >> 16) & 0xFF));
  out.push_back(static_cast<char>((value >> 8) & 0xFF));
  out.push_back(static_cast<char>(value & 0xFF));
  return out;
}

} // namespace

// 1. round-trip typical event fields
TEST_CASE("gossip_payload round-trip typical events", "[gossip_payload][roundtrip]") {
  SECTION("MESSAGE") {
    requireRoundTrip("MESSAGE", "n1-MESSAGE-alice-1-1", "alice", "hello", "1710000000");
  }
  SECTION("LOGIN") { requireRoundTrip("LOGIN", "n1-LOGIN-bob-1-2", "bob", "node-a", "0"); }
  SECTION("LOGOUT") { requireRoundTrip("LOGOUT", "n1-LOGOUT-bob-1-3", "bob", "node-a", "1"); }
  SECTION("USER_CREATED") {
    // content is an Argon2id-shaped hash on the wire (opaque bytes for encode/decode)
    requireRoundTrip("USER_CREATED", "n1-USER_CREATED-u-1-4", "u",
                     "$argon2id$v=19$m=65536,t=2,p=1$c29tZXNhbHQ$c29tZWhhc2g", "u@mail.test");
  }
  SECTION("ROOM_JOIN") {
    requireRoundTrip("ROOM_JOIN", "n1-ROOM_JOIN-alice-1-5", "alice", "node-a", "Lobby");
  }
}

// 2. empty fields
TEST_CASE("gossip_payload round-trip empty fields", "[gossip_payload][empty]") {
  SECTION("all empty") { requireRoundTrip("", "", "", "", ""); }
  SECTION("empty eventId") { requireRoundTrip("LOGIN", "", "user", "node", "0"); }
  SECTION("empty content") { requireRoundTrip("MESSAGE", "id", "user", "", "1"); }
  SECTION("empty field5") { requireRoundTrip("ROOM_LEAVE", "id", "user", "node", ""); }
}

// 3. pipes, braces, whitespace
TEST_CASE("gossip_payload round-trip pipes and whitespace", "[gossip_payload][special]") {
  SECTION("pipes in every field") {
    requireRoundTrip("A|B", "id|1", "user|name", "msg|with|pipes", "a|b");
  }
  SECTION("braces and parens") { requireRoundTrip("MESSAGE", "{id}", "(user)", "{hello}", "(ts)"); }
  SECTION("whitespace only") { requireRoundTrip("  ", "\t", " \n ", "  hi  ", " \t\n "); }
}

// 4. unicode and embedded nulls
TEST_CASE("gossip_payload round-trip unicode and null bytes", "[gossip_payload][bytes]") {
  SECTION("unicode") { requireRoundTrip("MESSAGE", "אירוע", "עֹמְרִי", "שלום 👋", "חדר"); }
  SECTION("embedded nulls") {
    const std::string type("M\0E", 3);
    const std::string eventId("id\0x", 4);
    const std::string username("u\0", 2);
    const std::string content("a\0b\0c", 5);
    const std::string field5("x\0y", 3);
    requireRoundTrip(type, eventId, username, content, field5);
  }
}

// 5. large content
TEST_CASE("gossip_payload round-trip large content", "[gossip_payload][large]") {
  const std::string big(64 * 1024, 'x');
  requireRoundTrip("MESSAGE", "big-id", "alice", big, "99");
}

// 6. wire size matches header + bodies
TEST_CASE("gossip_payload wire size is 20 plus field lengths", "[gossip_payload][size]") {
  const std::string type = "MESSAGE";
  const std::string eventId = "eid";
  const std::string username = "alice";
  const std::string content = "hi";
  const std::string field5 = "1";

  const std::string bytes = gossip_payload::encode(type, eventId, username, content, field5);
  REQUIRE(bytes.size() ==
          5 * 4 + type.size() + eventId.size() + username.size() + content.size() + field5.size());
}

// 7. decode rejects empty / short / truncated
TEST_CASE("gossip_payload decode rejects empty short truncated", "[gossip_payload][decode][edge]") {
  SECTION("empty") { REQUIRE_FALSE(gossip_payload::decode("").has_value()); }

  SECTION("shorter than one length prefix") {
    REQUIRE_FALSE(gossip_payload::decode(std::string("\x00\x00\x00", 3)).has_value());
  }

  SECTION("truncated after type length") {
    REQUIRE_FALSE(gossip_payload::decode(appendU32BE(5)).has_value());
  }

  SECTION("truncated mid body") {
    std::string partial = appendU32BE(5);
    partial += "MESS"; // claims MESSAGE (5) but only 4 bytes
    REQUIRE_FALSE(gossip_payload::decode(partial).has_value());
  }

  SECTION("truncated after type+eventId") {
    std::string partial = gossip_payload::encode("LOGIN", "eid", "", "", "");
    partial.resize(partial.size() - 12); // drop three trailing empty fields
    REQUIRE_FALSE(gossip_payload::decode(partial).has_value());
  }
}

// 8. decode rejects trailing junk / oversize length claim
TEST_CASE("gossip_payload decode rejects trailing junk and bad lengths",
          "[gossip_payload][decode][edge]") {
  SECTION("trailing junk") {
    std::string bytes = gossip_payload::encode("LOGIN", "eid", "u", "n", "0");
    bytes.push_back('X');
    REQUIRE_FALSE(gossip_payload::decode(bytes).has_value());
  }

  SECTION("length claims more than remaining") {
    std::string bad = appendU32BE(100);
    bad += "tiny";
    REQUIRE_FALSE(gossip_payload::decode(bad).has_value());
  }

  SECTION("zero length then truncated next header") {
    std::string bad = appendU32BE(0); // empty type
    bad += appendU32BE(4);            // eventId length 4
    bad += "ab";                      // only 2 of 4
    REQUIRE_FALSE(gossip_payload::decode(bad).has_value());
  }
}

// 9. eventId from full and partial payloads
TEST_CASE("gossip_payload eventId full and partial", "[gossip_payload][eventId]") {
  SECTION("full payload") {
    const std::string bytes =
        gossip_payload::encode("MESSAGE", "event-42", "alice", "hi|there", "7");
    REQUIRE(gossip_payload::eventId(bytes) == "event-42");
  }

  SECTION("partial type+id only still yields id") {
    std::string partial = gossip_payload::encode("LOGIN", "partial-id", "", "", "");
    partial.resize(partial.size() - 12);
    REQUIRE(gossip_payload::eventId(partial) == "partial-id");
    REQUIRE_FALSE(gossip_payload::decode(partial).has_value());
  }

  SECTION("empty payload") { REQUIRE(gossip_payload::eventId("").empty()); }

  SECTION("truncated before id") {
    std::string partial = appendU32BE(5);
    partial += "LOGIN";
    REQUIRE(gossip_payload::eventId(partial).empty());
  }

  SECTION("empty eventId field") {
    const std::string bytes = gossip_payload::encode("LOGIN", "", "u", "n", "0");
    REQUIRE(gossip_payload::eventId(bytes).empty());
  }

  SECTION("eventId with pipes and null") {
    const std::string id("a|b\0c", 5);
    const std::string bytes = gossip_payload::encode("MESSAGE", id, "u", "body", "1");
    REQUIRE(gossip_payload::eventId(bytes) == id);
  }
}

// 10. legacy pipe-delimited payload rejected
TEST_CASE("gossip_payload rejects legacy pipe format", "[gossip_payload][legacy]") {
  const std::string legacy = "MESSAGE|eid|alice|hello|1";
  REQUIRE_FALSE(gossip_payload::decode(legacy).has_value());
  REQUIRE(gossip_payload::eventId(legacy).empty());
}

// 11. double round-trip stability
TEST_CASE("gossip_payload double round-trip stable", "[gossip_payload][roundtrip]") {
  const std::string first = gossip_payload::encode("MESSAGE", "id", "alice", "msg|pipes\0x", "42");
  const auto once = gossip_payload::decode(first);
  REQUIRE(once.has_value());

  const std::string second = gossip_payload::encode(once->type, once->eventId, once->username,
                                                    once->content, once->field5);
  REQUIRE(second == first);

  const auto twice = gossip_payload::decode(second);
  REQUIRE(twice.has_value());
  requireFieldsEqual(*once, *twice);
}
