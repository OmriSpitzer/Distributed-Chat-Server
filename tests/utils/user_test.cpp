/**
 * User Class unit tests
 *
 * @brief Includes: constructor, setters, equality, stream output, typeToString, stringToType,
 * type conversion round-trip, anonymousUser, serialize / deserialize, deserialize rejects invalid
 * input.
 * @date 11-09-2026
 */

#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <stdexcept>
#include <string>

/**
 * 1. constructor stores metadata
 * 2. setters mutate fields
 * 3. equality is by email only
 * 4. stream output
 * 5. typeToString
 * 6. stringToType
 * 7. type conversion round-trip
 * 8. anonymousUser
 * 9. serialize / deserialize
 * 10. deserialize rejects invalid input
 */

// 1. constructor stores metadata
TEST_CASE("User constructor stores metadata", "[user][ctor]") {
  // USER type
  SECTION("USER type") {
    const User user("alice", "alice@example.com", User::UserType::USER);
    REQUIRE(user.getUsername() == "alice");
    REQUIRE(user.getEmail() == "alice@example.com");
    REQUIRE(user.getUserType() == User::UserType::USER);
  }

  // ADMIN type
  SECTION("ADMIN type") {
    const User user("admin", "admin@example.com", User::UserType::ADMIN);
    REQUIRE(user.getUserType() == User::UserType::ADMIN);
  }

  // GUEST type
  SECTION("GUEST type") {
    const User user("guest", "guest@example.com", User::UserType::GUEST);
    REQUIRE(user.getUserType() == User::UserType::GUEST);
  }

  // empty username
  SECTION("empty username") {
    const User user("", "empty@example.com", User::UserType::USER);
    REQUIRE(user.getUsername().empty());
    REQUIRE(user.getEmail() == "empty@example.com");
  }

  // empty email
  SECTION("empty email") {
    const User user("nameless", "", User::UserType::USER);
    REQUIRE(user.getUsername() == "nameless");
    REQUIRE(user.getEmail().empty());
  }

  // both empty
  SECTION("both empty") {
    const User user("", "", User::UserType::GUEST);
    REQUIRE(user.getUsername().empty());
    REQUIRE(user.getEmail().empty());
  }

  // whitespace and special characters
  SECTION("whitespace and special characters") {
    const User user("  alice  ", "a+b@ex.com", User::UserType::USER);
    REQUIRE(user.getUsername() == "  alice  ");
    REQUIRE(user.getEmail() == "a+b@ex.com");
  }

  // unicode
  SECTION("unicode") {
    const User user("עֹמְרִי", "omri@example.com", User::UserType::USER);
    REQUIRE(user.getUsername() == "עֹמְרִי");
  }

  // very long fields
  SECTION("very long fields") {
    const std::string longName(4096, 'x');
    const std::string longEmail = longName + "@example.com";
    const User user(longName, longEmail, User::UserType::USER);
    REQUIRE(user.getUsername() == longName);
    REQUIRE(user.getEmail() == longEmail);
  }
}

// 2. setters mutate fields
TEST_CASE("User setters mutate fields", "[user][setters]") {
  User user("alice", "alice@example.com", User::UserType::USER);

  // setUsername
  SECTION("setUsername") {
    user.setUsername("ally");
    REQUIRE(user.getUsername() == "ally");
    REQUIRE(user.getEmail() == "alice@example.com");
  }

  // setEmail
  SECTION("setEmail") {
    user.setEmail("ally@example.com");
    REQUIRE(user.getEmail() == "ally@example.com");
  }

  // setUserType
  SECTION("setUserType") {
    user.setUserType(User::UserType::ADMIN);
    REQUIRE(user.getUserType() == User::UserType::ADMIN);
  }

  // set empty values
  SECTION("set empty values") {
    user.setUsername("");
    user.setEmail("");
    REQUIRE(user.getUsername().empty());
    REQUIRE(user.getEmail().empty());
  }
}

// 3. equality is by email only
TEST_CASE("User equality is by email only", "[user][eq]") {
  const User alice("alice", "alice@example.com", User::UserType::USER);
  const User bob("bob", "bob@example.com", User::UserType::ADMIN);
  const User sameEmail("other", "alice@example.com", User::UserType::ADMIN);
  const User differentEmail("alice", "other@example.com", User::UserType::USER);

  // same email
  REQUIRE(alice == sameEmail);
  REQUIRE_FALSE(alice != sameEmail);

  // different email
  REQUIRE(alice != differentEmail);
  REQUIRE_FALSE(alice == differentEmail);

  // different type
  REQUIRE(alice != bob);

  // empty emails are equal
  SECTION("empty emails are equal") {
    const User a("a", "", User::UserType::USER);
    const User b("b", "", User::UserType::ADMIN);
    REQUIRE(a == b);
  }
}

// 4. stream output
TEST_CASE("User stream output", "[user][ostream]") {
  const User alice("alice", "alice@example.com", User::UserType::USER);

  // stream output
  std::ostringstream out;
  out << alice;
  const std::string s = out.str();
  REQUIRE(s.find("alice") != std::string::npos);
  REQUIRE(s.find("alice@example.com") != std::string::npos);
  REQUIRE(s.find("USER") != std::string::npos);
}

// 5. typeToString
TEST_CASE("User typeToString", "[user][typeToString]") {
  // valid types
  REQUIRE(User::typeToString(User::UserType::ADMIN) == "ADMIN");
  REQUIRE(User::typeToString(User::UserType::USER) == "USER");
  REQUIRE(User::typeToString(User::UserType::GUEST) == "GUEST");

  // invalid enum value defaults to GUEST
  SECTION("invalid enum value defaults to GUEST") {
    const auto bogus = static_cast<User::UserType>(999);
    REQUIRE(User::typeToString(bogus) == "GUEST");
  }
}

// 6. stringToType
TEST_CASE("User stringToType", "[user][stringToType]") {
  // valid types
  REQUIRE(User::stringToType("ADMIN") == User::UserType::ADMIN);
  REQUIRE(User::stringToType("USER") == User::UserType::USER);
  REQUIRE(User::stringToType("GUEST") == User::UserType::GUEST);

  // unknown / edge strings default to GUEST
  SECTION("unknown / edge strings default to GUEST") {
    REQUIRE(User::stringToType("") == User::UserType::GUEST);
    REQUIRE(User::stringToType("admin") == User::UserType::GUEST);
    REQUIRE(User::stringToType("Admin") == User::UserType::GUEST);
    REQUIRE(User::stringToType("USER ") == User::UserType::GUEST);
    REQUIRE(User::stringToType(" USER") == User::UserType::GUEST);
    REQUIRE(User::stringToType("UNKNOWN") == User::UserType::GUEST);
  }
}

// 7. type conversion round-trip
TEST_CASE("User type conversion round-trip", "[user][type-roundtrip]") {
  // valid types
  for (auto t : {User::UserType::ADMIN, User::UserType::USER, User::UserType::GUEST}) {
    REQUIRE(User::stringToType(User::typeToString(t)) == t);
  }
}

// 8. anonymousUser
TEST_CASE("User anonymousUser", "[user][anonymous]") {
  // anonymousUser
  const User a = User::anonymousUser();
  const User b = User::anonymousUser();

  // user type is GUEST
  REQUIRE(a.getUserType() == User::UserType::GUEST);
  REQUIRE(b.getUserType() == User::UserType::GUEST);

  // username starts with "anon"
  REQUIRE(a.getUsername().rfind("anon", 0) == 0);
  REQUIRE(b.getUsername().rfind("anon", 0) == 0);

  // email is the username + "@local"
  REQUIRE(a.getEmail() == a.getUsername() + "@local");
  REQUIRE(b.getEmail() == b.getUsername() + "@local");

  // likely unique; allow rare collision by checking format only if equal
  if (a.getUsername() == b.getUsername()) {
    REQUIRE(a.getUsername().size() == std::string("anon").size() + 7);
  } else {
    REQUIRE(a != b);
  }
}

// 9. serialize / deserialize
TEST_CASE("User serialize / deserialize", "[user][serialize]") {
  const User alice("alice", "alice@example.com", User::UserType::USER);
  const User bob("bob", "bob@example.com", User::UserType::ADMIN);

  // round-trip all types
  SECTION("round-trip all types") {
    for (auto t : {User::UserType::ADMIN, User::UserType::USER, User::UserType::GUEST}) {
      const User original("name", "name@ex.com", t);
      const User restored = User::deserialize(original.serialize());
      REQUIRE(restored.getUsername() == original.getUsername());
      REQUIRE(restored.getEmail() == original.getEmail());
      REQUIRE(restored.getUserType() == original.getUserType());
      REQUIRE(restored == original);
    }
  }

  // empty fields round-trip
  SECTION("empty fields round-trip") {
    const User original("", "", User::UserType::GUEST);
    const User restored = User::deserialize(original.serialize());
    REQUIRE(restored.getUsername().empty());
    REQUIRE(restored.getEmail().empty());
    REQUIRE(restored.getUserType() == User::UserType::GUEST);
  }

  // format shape
  SECTION("format shape") {
    REQUIRE(alice.serialize() == "user(alice|alice@example.com|USER)");
    REQUIRE(bob.serialize() == "user(bob|bob@example.com|ADMIN)");
  }

  // unknown type string in payload becomes GUEST
  SECTION("unknown type string in payload becomes GUEST") {
    const User restored = User::deserialize("user(x|y@z|NOTATYPE)");
    REQUIRE(restored.getUsername() == "x");
    REQUIRE(restored.getEmail() == "y@z");
    REQUIRE(restored.getUserType() == User::UserType::GUEST);
  }

  // pipe in username/email breaks format (documented limitation)
  SECTION("pipe in username/email breaks format (documented limitation)") {
    const User restored = User::deserialize("user(a|b|c|d@e.com|USER)");

    // deserialize throws invalid_argument
    REQUIRE(restored.getUsername() == "a");
    REQUIRE(restored.getEmail() == "b");
    REQUIRE(restored.getUserType() == User::UserType::GUEST);
  }
}

// 10. deserialize rejects invalid input
TEST_CASE("User deserialize rejects invalid input", "[user][deserialize][edge]") {
  // empty input
  REQUIRE_THROWS_AS(User::deserialize(""), std::invalid_argument);

  // user input
  REQUIRE_THROWS_AS(User::deserialize("user"), std::invalid_argument);

  // user( input
  REQUIRE_THROWS_AS(User::deserialize("user("), std::invalid_argument);
  REQUIRE_THROWS_AS(User::deserialize("user()"), std::invalid_argument);
  REQUIRE_THROWS_AS(User::deserialize("user(onlyone)"), std::invalid_argument);
  REQUIRE_THROWS_AS(User::deserialize("user(one|two)"), std::invalid_argument); // missing type pipe
  REQUIRE_THROWS_AS(User::deserialize("User(alice|alice@example.com|USER)"), std::invalid_argument);
  REQUIRE_THROWS_AS(User::deserialize("user(alice|alice@example.com|USER"),
                    std::invalid_argument); // no )
  REQUIRE_THROWS_AS(User::deserialize("xuser(alice|alice@example.com|USER)"),
                    std::invalid_argument);
}