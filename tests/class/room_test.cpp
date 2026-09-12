/**
 * Room Class unit tests
 *
 * @brief Includes: constructor, setters, equality, stream output, typeToString, stringToType,
 * type conversion round-trip.
 * @date 11-09-2026
 */

#include "utils/models/room.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * 1. constructor stores metadata
 * 2. successive rooms receive unique ids
 * 3. room copy keeps metadata
 * 4. room typeToString
 * 5. room stringToType
 * 6. room type conversion round-trip
 * 7. room privacyToString
 * 8. room stringToPrivacy
 * 9. room privacy conversion round-trip
 */

// 1. constructor stores metadata
TEST_CASE("Room constructor stores metadata", "[room][ctor]") {
  // defaults
  SECTION("defaults") {
    const Room room("general");
    REQUIRE(room.getName() == "general");
    REQUIRE(room.getType() == Room::RoomType::OTHER);
    REQUIRE(room.getPrivacy() == Room::Privacy::PUBLIC);
    REQUIRE_FALSE(room.getId().empty());
  }

  // explicit type and privacy
  SECTION("explicit type and privacy") {
    const Room room("secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);
    REQUIRE(room.getName() == "secure");
    REQUIRE(room.getType() == Room::RoomType::SECURITY);
    REQUIRE(room.getPrivacy() == Room::Privacy::PRIVATE);
  }

  // lobby
  SECTION("lobby") {
    const Room room("Lobby", Room::RoomType::LOBBY);
    REQUIRE(room.getName() == "Lobby");
    REQUIRE(room.getType() == Room::RoomType::LOBBY);
    REQUIRE(room.getPrivacy() == Room::Privacy::PUBLIC);
  }

  // empty name
  SECTION("empty name") {
    const Room room("");
    REQUIRE(room.getName().empty());
    REQUIRE_FALSE(room.getId().empty());
  }

  // whitespace name
  SECTION("whitespace name") {
    const Room room("  room  ");
    REQUIRE(room.getName() == "  room  ");
  }

  // unicode name
  SECTION("unicode name") {
    const Room room("חדר");
    REQUIRE(room.getName() == "חדר");
  }

  // long name
  SECTION("long name") {
    const std::string longName(4096, 'r');
    const Room room(longName, Room::RoomType::QA, Room::Privacy::PRIVATE);
    REQUIRE(room.getName() == longName);
    REQUIRE(room.getType() == Room::RoomType::QA);
    REQUIRE(room.getPrivacy() == Room::Privacy::PRIVATE);
  }

  // all room types construct
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
      const Room room("t", type);
      REQUIRE(room.getType() == type);
    }
  }
}

// 2. successive rooms receive unique ids
TEST_CASE("Successive rooms receive unique ids", "[room][id]") {
  const Room first("one");
  const Room second("two");
  const Room third("one"); // same name, different instance

  // check unique ids
  REQUIRE(first.getId() != second.getId());
  REQUIRE(first.getId() != third.getId());
  REQUIRE(second.getId() != third.getId());

  // many rooms stay unique
  SECTION("many rooms stay unique") {
    std::unordered_set<std::string> ids;
    for (int i = 0; i < 100; ++i) {
      const Room room("r" + std::to_string(i));
      REQUIRE(ids.insert(room.getId()).second);
    }
  }
}

// 3. room copy keeps metadata
TEST_CASE("Room copy keeps metadata", "[room][copy]") {
  const Room original("general", Room::RoomType::DEVOPS, Room::Privacy::PRIVATE);
  const Room copy = original;

  // check metadata
  REQUIRE(copy.getId() == original.getId());
  REQUIRE(copy.getName() == original.getName());
  REQUIRE(copy.getType() == original.getType());
  REQUIRE(copy.getPrivacy() == original.getPrivacy());
}

// 4. room typeToString
TEST_CASE("Room roomTypeToString", "[room][typeToString]") {
  // check all room types
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

  // invalid enum defaults to Other
  SECTION("invalid enum defaults to Other") {
    const auto bogus = static_cast<Room::RoomType>(999);
    REQUIRE(Room::roomTypeToString(bogus) == "Other");
  }
}

// 5. room stringToType
TEST_CASE("Room stringToRoomType", "[room][stringToType]") {
  // check all room types
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

  // unknown / edge strings default to OTHER
  SECTION("unknown / edge strings default to OTHER") {
    REQUIRE(Room::stringToRoomType("") == Room::RoomType::OTHER);
    REQUIRE(Room::stringToRoomType("lobby") == Room::RoomType::OTHER); // case-sensitive
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

  // check conversion round-trip
  for (auto type : types) {
    REQUIRE(Room::stringToRoomType(Room::roomTypeToString(type)) == type);
  }
}

// 7. room privacyToString
TEST_CASE("Room privacyToString", "[room][privacyToString]") {
  // check all privacy types
  REQUIRE(Room::privacyToString(Room::Privacy::PUBLIC) == "PUBLIC");
  REQUIRE(Room::privacyToString(Room::Privacy::PRIVATE) == "PRIVATE");

  // invalid enum defaults to PUBLIC
  SECTION("invalid enum defaults to PUBLIC") {
    const auto bogus = static_cast<Room::Privacy>(999);
    REQUIRE(Room::privacyToString(bogus) == "PUBLIC");
  }
}

// 8. room stringToPrivacy
TEST_CASE("Room stringToPrivacy", "[room][stringToPrivacy]") {
  // check all privacy types
  REQUIRE(Room::stringToPrivacy("PUBLIC") == Room::Privacy::PUBLIC);
  REQUIRE(Room::stringToPrivacy("PRIVATE") == Room::Privacy::PRIVATE);

  // unknown / edge strings default to PUBLIC
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
  // check conversion round-trip
  REQUIRE(Room::stringToPrivacy(Room::privacyToString(Room::Privacy::PUBLIC)) ==
          Room::Privacy::PUBLIC);
  REQUIRE(Room::stringToPrivacy(Room::privacyToString(Room::Privacy::PRIVATE)) ==
          Room::Privacy::PRIVATE);
}

// 10. room stream output includes name, type, and privacy
TEST_CASE("Room stream output includes name, type, and privacy", "[room][print]") {
  // public other
  SECTION("public other") {
    const Room room("general");
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("general") != std::string::npos);
    REQUIRE(text.find(room.getId()) != std::string::npos);
    REQUIRE(text.find("Other") != std::string::npos);
    REQUIRE(text.find("PUBLIC") != std::string::npos);
  }

  // private security
  SECTION("private security") {
    const Room room("secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("secure") != std::string::npos);
    REQUIRE(text.find("Security") != std::string::npos);
    REQUIRE(text.find("PRIVATE") != std::string::npos);
  }

  // lobby
  SECTION("lobby") {
    const Room room("Lobby", Room::RoomType::LOBBY);
    std::ostringstream out;
    out << room;
    const std::string text = out.str();

    REQUIRE(text.find("Lobby") != std::string::npos);
  }
}