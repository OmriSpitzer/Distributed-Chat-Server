/**
 * Room Class unit tests
 *
 * @brief Includes: constructor (explicit DB id), equality by id, stream output,
 * typeToString, stringToType, type/privacy conversion round-trip.
 * Room ids are assigned by SQLite AUTOINCREMENT — the ctor only stores a given id.
 * @date 11-09-2026
 */

#include "utils/models/room.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * 1. constructor stores metadata including explicit id
 * 2. equality is by id (DB primary key)
 * 3. room copy keeps metadata
 * 4. room typeToString
 * 5. room stringToType
 * 6. room type conversion round-trip
 * 7. room privacyToString
 * 8. room stringToPrivacy
 * 9. room privacy conversion round-trip
 * 10. stream output
 */

// 1. constructor stores metadata including explicit id
TEST_CASE("Room constructor stores metadata", "[room][ctor]") {
  SECTION("defaults") {
    const Room room(1, "general");
    REQUIRE(room.getId() == 1);
    REQUIRE(room.getName() == "general");
    REQUIRE(room.getType() == Room::RoomType::OTHER);
    REQUIRE(room.getPrivacy() == Room::Privacy::PUBLIC);
  }

  SECTION("explicit type and privacy") {
    const Room room(42, "secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);
    REQUIRE(room.getId() == 42);
    REQUIRE(room.getName() == "secure");
    REQUIRE(room.getType() == Room::RoomType::SECURITY);
    REQUIRE(room.getPrivacy() == Room::Privacy::PRIVATE);
  }

  SECTION("lobby seed id") {
    const Room room(1, "Lobby", Room::RoomType::LOBBY);
    REQUIRE(room.getId() == 1);
    REQUIRE(room.getName() == "Lobby");
    REQUIRE(room.getType() == Room::RoomType::LOBBY);
    REQUIRE(room.getPrivacy() == Room::Privacy::PUBLIC);
  }

  SECTION("empty name") {
    const Room room(7, "");
    REQUIRE(room.getId() == 7);
    REQUIRE(room.getName().empty());
  }

  SECTION("whitespace name") {
    const Room room(3, "  room  ");
    REQUIRE(room.getName() == "  room  ");
  }

  SECTION("unicode name") {
    const Room room(4, "חדר");
    REQUIRE(room.getName() == "חדר");
  }

  SECTION("long name") {
    const std::string longName(4096, 'r');
    const Room room(5, longName, Room::RoomType::QA, Room::Privacy::PRIVATE);
    REQUIRE(room.getName() == longName);
    REQUIRE(room.getType() == Room::RoomType::QA);
    REQUIRE(room.getPrivacy() == Room::Privacy::PRIVATE);
  }

  SECTION("all room types construct") {
    const std::vector<Room::RoomType> types = {
        Room::RoomType::RESEARCH_AND_DEVELOPMENT,
        Room::RoomType::PRODUCTION,
        Room::RoomType::QA,
        Room::RoomType::DEVOPS,
        Room::RoomType::SECURITY,
        Room::RoomType::DESIGN,
        Room::RoomType::MARKETING,
        Room::RoomType::HR,
        Room::RoomType::FINANCE,
        Room::RoomType::LEGAL,
        Room::RoomType::CUSTOMER_SUPPORT,
        Room::RoomType::OTHER,
        Room::RoomType::LOBBY,
    };
    for (auto type : types) {
      const Room room(9, "t", type);
      REQUIRE(room.getType() == type);
    }
  }
}

// 2. equality is by id (DB primary key) — not by name
TEST_CASE("Room equality compares id", "[room][id][equality]") {
  const Room a(1, "one");
  const Room b(2, "two");
  const Room sameIdDifferentName(1, "other");

  REQUIRE(a != b);
  REQUIRE(a == sameIdDifferentName);
  REQUIRE_FALSE(a == b);

  SECTION("copy keeps same id") {
    const Room copy = a;
    REQUIRE(copy == a);
    REQUIRE(copy.getId() == a.getId());
  }
}

// 3. room copy keeps metadata
TEST_CASE("Room copy keeps metadata", "[room][copy]") {
  const Room original(10, "general", Room::RoomType::DEVOPS, Room::Privacy::PRIVATE);
  const Room copy = original;

  REQUIRE(copy.getId() == original.getId());
  REQUIRE(copy.getName() == original.getName());
  REQUIRE(copy.getType() == original.getType());
  REQUIRE(copy.getPrivacy() == original.getPrivacy());
}

// 4. room typeToString
TEST_CASE("Room roomTypeToString", "[room][typeToString]") {
  REQUIRE(Room::roomTypeToString(Room::RoomType::RESEARCH_AND_DEVELOPMENT) == "R&D");
  REQUIRE(Room::roomTypeToString(Room::RoomType::PRODUCTION) == "Production");
  REQUIRE(Room::roomTypeToString(Room::RoomType::QA) == "QA");
  REQUIRE(Room::roomTypeToString(Room::RoomType::DEVOPS) == "DevOps");
  REQUIRE(Room::roomTypeToString(Room::RoomType::SECURITY) == "Security");
  REQUIRE(Room::roomTypeToString(Room::RoomType::DESIGN) == "Design");
  REQUIRE(Room::roomTypeToString(Room::RoomType::MARKETING) == "Marketing");
  REQUIRE(Room::roomTypeToString(Room::RoomType::HR) == "HR");
  REQUIRE(Room::roomTypeToString(Room::RoomType::FINANCE) == "Finance");
  REQUIRE(Room::roomTypeToString(Room::RoomType::LEGAL) == "Legal");
  REQUIRE(Room::roomTypeToString(Room::RoomType::CUSTOMER_SUPPORT) == "Customer Support");
  REQUIRE(Room::roomTypeToString(Room::RoomType::OTHER) == "Other");
  REQUIRE(Room::roomTypeToString(Room::RoomType::LOBBY) == "Lobby");

  SECTION("invalid enum defaults to Other") {
    const auto bogus = static_cast<Room::RoomType>(999);
    REQUIRE(Room::roomTypeToString(bogus) == "Other");
  }
}

// 5. room stringToType
TEST_CASE("Room stringToRoomType", "[room][stringToType]") {
  REQUIRE(Room::stringToRoomType("R&D") == Room::RoomType::RESEARCH_AND_DEVELOPMENT);
  REQUIRE(Room::stringToRoomType("Production") == Room::RoomType::PRODUCTION);
  REQUIRE(Room::stringToRoomType("QA") == Room::RoomType::QA);
  REQUIRE(Room::stringToRoomType("DevOps") == Room::RoomType::DEVOPS);
  REQUIRE(Room::stringToRoomType("Security") == Room::RoomType::SECURITY);
  REQUIRE(Room::stringToRoomType("Design") == Room::RoomType::DESIGN);
  REQUIRE(Room::stringToRoomType("Marketing") == Room::RoomType::MARKETING);
  REQUIRE(Room::stringToRoomType("HR") == Room::RoomType::HR);
  REQUIRE(Room::stringToRoomType("Finance") == Room::RoomType::FINANCE);
  REQUIRE(Room::stringToRoomType("Legal") == Room::RoomType::LEGAL);
  REQUIRE(Room::stringToRoomType("Customer Support") == Room::RoomType::CUSTOMER_SUPPORT);
  REQUIRE(Room::stringToRoomType("Other") == Room::RoomType::OTHER);
  REQUIRE(Room::stringToRoomType("Lobby") == Room::RoomType::LOBBY);

  SECTION("unknown / edge strings default to OTHER") {
    REQUIRE(Room::stringToRoomType("") == Room::RoomType::OTHER);
    REQUIRE(Room::stringToRoomType("lobby") == Room::RoomType::OTHER);
    REQUIRE(Room::stringToRoomType("LOBBY") == Room::RoomType::OTHER);
    REQUIRE(Room::stringToRoomType("QA ") == Room::RoomType::OTHER);
    REQUIRE(Room::stringToRoomType("Unknown") == Room::RoomType::OTHER);
  }
}

// 6. room type conversion round-trip
TEST_CASE("Room type conversion round-trip", "[room][type-roundtrip]") {
  const std::vector<Room::RoomType> types = {
      Room::RoomType::RESEARCH_AND_DEVELOPMENT,
      Room::RoomType::PRODUCTION,
      Room::RoomType::QA,
      Room::RoomType::DEVOPS,
      Room::RoomType::SECURITY,
      Room::RoomType::DESIGN,
      Room::RoomType::MARKETING,
      Room::RoomType::HR,
      Room::RoomType::FINANCE,
      Room::RoomType::LEGAL,
      Room::RoomType::CUSTOMER_SUPPORT,
      Room::RoomType::OTHER,
      Room::RoomType::LOBBY,
  };

  for (auto type : types) {
    REQUIRE(Room::stringToRoomType(Room::roomTypeToString(type)) == type);
  }
}

// 7. room privacyToString
TEST_CASE("Room privacyToString", "[room][privacyToString]") {
  REQUIRE(Room::privacyToString(Room::Privacy::PUBLIC) == "PUBLIC");
  REQUIRE(Room::privacyToString(Room::Privacy::PRIVATE) == "PRIVATE");

  SECTION("invalid enum defaults to PUBLIC") {
    const auto bogus = static_cast<Room::Privacy>(999);
    REQUIRE(Room::privacyToString(bogus) == "PUBLIC");
  }
}

// 8. room stringToPrivacy
TEST_CASE("Room stringToPrivacy", "[room][stringToPrivacy]") {
  REQUIRE(Room::stringToPrivacy("PUBLIC") == Room::Privacy::PUBLIC);
  REQUIRE(Room::stringToPrivacy("PRIVATE") == Room::Privacy::PRIVATE);

  SECTION("unknown / edge strings default to PUBLIC") {
    REQUIRE(Room::stringToPrivacy("") == Room::Privacy::PUBLIC);
    REQUIRE(Room::stringToPrivacy("public") == Room::Privacy::PUBLIC);
    REQUIRE(Room::stringToPrivacy("Private") == Room::Privacy::PUBLIC);
    REQUIRE(Room::stringToPrivacy("PRIVATE ") == Room::Privacy::PUBLIC);
    REQUIRE(Room::stringToPrivacy("SECRET") == Room::Privacy::PUBLIC);
  }
}

// 9. room privacy conversion round-trip
TEST_CASE("Room privacy conversion round-trip", "[room][privacy-roundtrip]") {
  REQUIRE(Room::stringToPrivacy(Room::privacyToString(Room::Privacy::PUBLIC)) ==
          Room::Privacy::PUBLIC);
  REQUIRE(Room::stringToPrivacy(Room::privacyToString(Room::Privacy::PRIVATE)) ==
          Room::Privacy::PRIVATE);
}

// 10. room stream output includes name, type, id, and privacy
TEST_CASE("Room stream output includes name, type, and privacy", "[room][print]") {
  SECTION("public other") {
    const Room room(11, "general");
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("general") != std::string::npos);
    REQUIRE(text.find("11") != std::string::npos);
    REQUIRE(text.find("Other") != std::string::npos);
    REQUIRE(text.find("PUBLIC") != std::string::npos);
  }

  SECTION("private security") {
    const Room room(12, "secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("secure") != std::string::npos);
    REQUIRE(text.find("Security") != std::string::npos);
    REQUIRE(text.find("PRIVATE") != std::string::npos);
  }

  SECTION("lobby") {
    const Room room(1, "Lobby", Room::RoomType::LOBBY);
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("Lobby") != std::string::npos);
  }
}

// 11. room serialize / deserialize round-trip
TEST_CASE("Room serialize deserialize round-trip", "[room][serialize]") {
  const Room original(42, "secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);
  const std::string bytes = original.serialize();
  REQUIRE(bytes == "room(42|secure|Security|PRIVATE)");

  const Room restored = Room::deserialize(bytes);
  REQUIRE(restored.getId() == 42);
  REQUIRE(restored.getName() == "secure");
  REQUIRE(restored.getType() == Room::RoomType::SECURITY);
  REQUIRE(restored.getPrivacy() == Room::Privacy::PRIVATE);
}

// 12. room list serialize / deserialize
TEST_CASE("Room serializeList deserializeList", "[room][serialize][list]") {
  const std::vector<Room> rooms = {
      Room(1, "Lobby", Room::RoomType::LOBBY),
      Room(2, "General", Room::RoomType::OTHER),
  };

  const std::string encoded = Room::serializeList(rooms);
  const std::vector<Room> restored = Room::deserializeList(encoded);

  REQUIRE(restored.size() == 2);
  REQUIRE(restored[0].getName() == "Lobby");
  REQUIRE(restored[1].getName() == "General");

  SECTION("empty list") {
    REQUIRE(Room::serializeList({}).empty());
    REQUIRE(Room::deserializeList("").empty());
  }

  SECTION("invalid entry throws") {
    REQUIRE_THROWS_AS(Room::deserializeList("not-a-room"), std::invalid_argument);
  }
}
