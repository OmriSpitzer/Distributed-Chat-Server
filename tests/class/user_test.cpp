/**
 * User Class unit tests
 */

#include "utils/models/user.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

static const User alice("alice", "alice@example.com", User::UserType::USER);
static const User bob("bob", "bob@example.com", User::UserType::USER);

TEST_CASE("User stores username, email, and type", "[user][ctor]") {
  SECTION("USER") {
    const User user("alice", "alice@example.com", User::UserType::USER);
    REQUIRE(user.getUsername() == "alice");
    REQUIRE(user.getEmail() == "alice@example.com");
    REQUIRE(user.getUserType() == User::UserType::USER);
  }

  SECTION("ADMIN") {
    const User user("admin", "admin@example.com", User::UserType::ADMIN);
    REQUIRE(user.getUsername() == "admin");
    REQUIRE(user.getEmail() == "admin@example.com");
    REQUIRE(user.getUserType() == User::UserType::ADMIN);
  }

  SECTION("GUEST") {
    const User user("guest", "guest@example.com", User::UserType::GUEST);
    REQUIRE(user.getUsername() == "guest");
    REQUIRE(user.getEmail() == "guest@example.com");
    REQUIRE(user.getUserType() == User::UserType::GUEST);
  }

  SECTION("empty username") {
    const User user("", "empty@example.com", User::UserType::USER);
    REQUIRE(user.getUsername().empty());
    REQUIRE(user.getEmail() == "empty@example.com");
  }

  SECTION("empty email") {
    const User user("nameless", "", User::UserType::USER);
    REQUIRE(user.getUsername() == "nameless");
    REQUIRE(user.getEmail().empty());
  }
}

TEST_CASE("Users with the same email are equal", "[user][equality]") {
  const User same_email("other", "alice@example.com", User::UserType::ADMIN);

  REQUIRE(alice == alice);
  REQUIRE(alice == same_email);
  REQUIRE_FALSE(alice != same_email);
  REQUIRE(alice != bob);
  REQUIRE_FALSE(alice == bob);
}

TEST_CASE("User setters update fields", "[user][setters]") {
  User user("alice", "alice@example.com", User::UserType::USER);

  SECTION("username") {
    user.setUsername("ally");
    REQUIRE(user.getUsername() == "ally");
    REQUIRE(user.getEmail() == "alice@example.com");
  }

  SECTION("email") {
    user.setEmail("ally@example.com");
    REQUIRE(user.getEmail() == "ally@example.com");
    REQUIRE(user != alice);
  }

  SECTION("type") {
    user.setUserType(User::UserType::ADMIN);
    REQUIRE(user.getUserType() == User::UserType::ADMIN);
  }
}

TEST_CASE("User stream output includes username, email, and type", "[user][print]") {
  SECTION("USER") {
    std::ostringstream out;
    out << alice;
    const std::string text = out.str();

    REQUIRE(text.find("alice") != std::string::npos);
    REQUIRE(text.find("alice@example.com") != std::string::npos);
    REQUIRE(text.find("USER") != std::string::npos);
  }

  SECTION("ADMIN") {
    const User admin("admin", "admin@example.com", User::UserType::ADMIN);
    std::ostringstream out;
    out << admin;
    const std::string text = out.str();

    REQUIRE(text.find("admin") != std::string::npos);
    REQUIRE(text.find("ADMIN") != std::string::npos);
  }

  SECTION("GUEST") {
    const User guest("guest", "guest@example.com", User::UserType::GUEST);
    std::ostringstream out;
    out << guest;
    const std::string text = out.str();

    REQUIRE(text.find("guest") != std::string::npos);
    REQUIRE(text.find("GUEST") != std::string::npos);
  }
}
