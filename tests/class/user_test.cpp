/**
 * User test file
 *
 * @brief Test cases for the User class
 * @date 12-07-2026
 */

#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::string captureStream(const User &user) {
  std::ostringstream stream;
  stream << user;
  return stream.str();
}

} // namespace

// test the constructor of the User class
TEST_CASE("Constructor Test", "[User][constructor]") {
  // test the ADMIN user
  SECTION("ADMIN user") {
    User user("admin", "admin@example.com", User::UserType::ADMIN);
    REQUIRE(user.getUsername() == "admin");
    REQUIRE(user.getEmail() == "admin@example.com");
    REQUIRE(user.getUserType() == User::UserType::ADMIN);
  }

  // test the USER user
  SECTION("USER user") {
    User user("alice", "alice@example.com", User::UserType::USER);
    REQUIRE(user.getUsername() == "alice");
    REQUIRE(user.getEmail() == "alice@example.com");
    REQUIRE(user.getUserType() == User::UserType::USER);
  }

  // test the GUEST user
  SECTION("GUEST user") {
    User user("guest", "guest@example.com", User::UserType::GUEST);
    REQUIRE(user.getUsername() == "guest");
    REQUIRE(user.getEmail() == "guest@example.com");
    REQUIRE(user.getUserType() == User::UserType::GUEST);
  }
}

// test the edge cases of the User class
TEST_CASE("Constructor Edge Cases", "[User][constructor][edge]") {
  // test the empty username and email
  SECTION("empty username and email") {
    User user("", "", User::UserType::USER);
    REQUIRE(user.getUsername().empty());
    REQUIRE(user.getEmail().empty());
    REQUIRE(user.getUserType() == User::UserType::USER);
  }

  // test the whitespace-only username
  SECTION("whitespace-only username") {
    User user("   \t\n  ", "user@example.com", User::UserType::GUEST);
    REQUIRE(user.getUsername() == "   \t\n  ");
    REQUIRE(user.getEmail() == "user@example.com");
  }

  // test the very long username and email
  SECTION("very long username and email") {
    const std::string longUsername(5'000, 'u');
    const std::string longEmail(5'000, 'e');
    User user(longUsername, longEmail, User::UserType::ADMIN);
    REQUIRE(user.getUsername() == longUsername);
    REQUIRE(user.getEmail() == longEmail);
    REQUIRE(user.getUsername().size() == 5'000);
    REQUIRE(user.getEmail().size() == 5'000);
  }

  // test the special characters and unicode
  SECTION("special characters and unicode") {
    const std::string username = "user_name+tag";
    const std::string email = "user+tag@example.co.uk";
    User user(username, email, User::UserType::USER);
    REQUIRE(user.getUsername() == username);
    REQUIRE(user.getEmail() == email);
  }

  // test the string_view slice without null terminator in source range
  SECTION("string_view slice without null terminator in source range") {
    const std::string usernameSource = "prefix:alice:suffix";
    const std::string emailSource = "before:bob@example.com:after";
    User user(std::string_view(usernameSource).substr(7, 5),
              std::string_view(emailSource).substr(7, 15), User::UserType::USER);
    REQUIRE(user.getUsername() == "alice");
    REQUIRE(user.getEmail() == "bob@example.com");
  }
}

// test the setters of the User class
TEST_CASE("Setters Test", "[User][setters]") {
  User user("original", "original@example.com", User::UserType::USER);

  // test the setUsername with normal text
  SECTION("setUsername with normal text") {
    user.setUsername("updated");
    REQUIRE(user.getUsername() == "updated");
  }

  // test the setUsername with empty string
  SECTION("setUsername with empty string") {
    user.setUsername("");
    REQUIRE(user.getUsername().empty());
  }

  // test the setUsername with string_view slice
  SECTION("setUsername with string_view slice") {
    const std::string source = "before-after";
    user.setUsername(std::string_view(source).substr(7));
    REQUIRE(user.getUsername() == "after");
  }

  // test the setEmail with normal text
  SECTION("setEmail with normal text") {
    user.setEmail("new@example.com");
    REQUIRE(user.getEmail() == "new@example.com");
  }

  // test the setEmail with empty string
  SECTION("setEmail with empty string") {
    user.setEmail("");
    REQUIRE(user.getEmail().empty());
  }

  // test the setEmail with string_view slice
  SECTION("setEmail with string_view slice") {
    const std::string source = "prefix:slice@example.com:suffix";
    user.setEmail(std::string_view(source).substr(7, 17));
    REQUIRE(user.getEmail() == "slice@example.com");
  }

  // test the setUserType cycles through all values
  SECTION("setUserType cycles through all values") {
    user.setUserType(User::UserType::ADMIN);
    REQUIRE(user.getUserType() == User::UserType::ADMIN);

    user.setUserType(User::UserType::GUEST);
    REQUIRE(user.getUserType() == User::UserType::GUEST);

    user.setUserType(User::UserType::USER);
    REQUIRE(user.getUserType() == User::UserType::USER);
  }

  // test the setters do not affect unrelated fields
  SECTION("setters do not affect unrelated fields") {
    user.setUsername("name-only");
    REQUIRE(user.getEmail() == "original@example.com");
    REQUIRE(user.getUserType() == User::UserType::USER);

    user.setEmail("email-only@example.com");
    REQUIRE(user.getUsername() == "name-only");
    REQUIRE(user.getUserType() == User::UserType::USER);

    user.setUserType(User::UserType::ADMIN);
    REQUIRE(user.getUsername() == "name-only");
    REQUIRE(user.getEmail() == "email-only@example.com");
  }
}

// test the getters of the User class
TEST_CASE("Getters Test", "[User][getters]") {
  User user("start", "start@example.com", User::UserType::GUEST);

  // test the getUsername returns the correct value
  SECTION("getUsername returns the correct value") {
    user.setUsername("end");
    REQUIRE(user.getUsername() == "end");
  }

  // test the getEmail returns the correct value
  SECTION("getEmail returns the correct value") {
    user.setEmail("end@example.com");
    REQUIRE(user.getEmail() == "end@example.com");
  }

  // test the getUserType returns the correct value
  SECTION("getUserType returns the correct value") {
    user.setUserType(User::UserType::ADMIN);
    REQUIRE(user.getUserType() == User::UserType::ADMIN);
  }

  // test the getters return consistent values after updates
  SECTION("getters return consistent values after updates") {
    user.setUsername("final");
    user.setEmail("final@example.com");
    user.setUserType(User::UserType::USER);

    REQUIRE(user.getUsername() == "final");
    REQUIRE(user.getEmail() == "final@example.com");
    REQUIRE(user.getUserType() == User::UserType::USER);
  }
}

// test the equality of the User class
TEST_CASE("Equality Test", "[User][equality]") {
  User first("alice", "alice@example.com", User::UserType::ADMIN);
  User second("bob", "bob@example.com", User::UserType::USER);

  REQUIRE_FALSE(first == second);
  REQUIRE(first != second);

  // test the same object is equal to itself
  SECTION("same object is equal to itself") {
    REQUIRE(first == first);
    REQUIRE_FALSE(first != first);
  }

  // test the copied user with same email is equal
  SECTION("copied user with same email is equal") {
    const User copy = first;
    REQUIRE(copy == first);
    REQUIRE(copy.getUsername() == first.getUsername());
    REQUIRE(copy.getEmail() == first.getEmail());
    REQUIRE(copy.getUserType() == first.getUserType());
  }

  // test the different emails are not equal
  SECTION("different emails are not equal") {
    User a("user-a", "a@example.com", User::UserType::USER);
    User b("user-b", "b@example.com", User::UserType::USER);
    REQUIRE_FALSE(a == b);
    REQUIRE(a != b);
  }

  // test the equality compares email only
  SECTION("equality compares email only") {
    User original("alpha", "same@example.com", User::UserType::ADMIN);
    User sameEmail("beta", "same@example.com", User::UserType::GUEST);

    REQUIRE(sameEmail == original);
    REQUIRE(sameEmail.getUsername() != original.getUsername());
    REQUIRE(sameEmail.getUserType() != original.getUserType());
  }

  // test the users with same username and type but different emails are not equal
  SECTION("users with same username and type but different emails are not equal") {
    User a("shared", "a@example.com", User::UserType::USER);
    User b("shared", "b@example.com", User::UserType::USER);
    REQUIRE_FALSE(a == b);
    REQUIRE(a != b);
  }

  // test the operator!= is consistent with operator==
  SECTION("operator!= is consistent with operator==") {
    User a("one", "one@example.com", User::UserType::USER);
    User b("two", "two@example.com", User::UserType::USER);
    User c("three", "one@example.com", User::UserType::GUEST);

    REQUIRE((a != b) == !(a == b));
    REQUIRE((a != c) == !(a == c));
  }
}

// test the stream output format of the User class
TEST_CASE("Stream Output Format", "[User][ostream]") {
  User user("stream-user", "stream@example.com", User::UserType::ADMIN);
  const std::string output = captureStream(user);

  REQUIRE(output == "User: stream-user (stream@example.com) - ADMIN");
}

// test the stream output type labels of the User class
TEST_CASE("Stream Output Type Labels", "[User][ostream][types]") {
  // test the ADMIN label
  SECTION("ADMIN label") {
    User user("admin", "admin@example.com", User::UserType::ADMIN);
    REQUIRE(captureStream(user).find("- ADMIN") != std::string::npos);
  }

  // test the USER label
  SECTION("USER label") {
    User user("member", "member@example.com", User::UserType::USER);
    REQUIRE(captureStream(user).find("- USER") != std::string::npos);
  }

  // test the GUEST label
  SECTION("GUEST label") {
    User user("visitor", "visitor@example.com", User::UserType::GUEST);
    REQUIRE(captureStream(user).find("- GUEST") != std::string::npos);
  }
}

// test the stream output edge cases of the User class
TEST_CASE("Stream Output Edge Cases", "[User][ostream][edge]") {
  // test the empty username and email still print format
  SECTION("empty username and email still print format") {
    User user("", "", User::UserType::USER);
    const std::string output = captureStream(user);
    REQUIRE(output == "User:  () - USER");
  }

  // test the updated fields appear in output
  SECTION("updated fields appear in output") {
    User user("old", "old@example.com", User::UserType::GUEST);
    user.setUsername("new");
    user.setEmail("new@example.com");
    user.setUserType(User::UserType::ADMIN);
    const std::string output = captureStream(user);
    REQUIRE(output == "User: new (new@example.com) - ADMIN");
  }

  // test the special characters are preserved in output
  SECTION("special characters are preserved in output") {
    User user("user_name", "user+tag@example.com", User::UserType::USER);
    const std::string output = captureStream(user);
    REQUIRE(output.find("user_name") != std::string::npos);
    REQUIRE(output.find("user+tag@example.com") != std::string::npos);
  }
}

// test the copy semantics of the User class
TEST_CASE("Copy Semantics Test", "[User][copy]") {
  User original("copy-me", "copy@example.com", User::UserType::ADMIN);
  const std::string originalUsername = original.getUsername();
  const std::string originalEmail = original.getEmail();
  const auto originalType = original.getUserType();

  User copy = original;

  REQUIRE(copy.getUsername() == originalUsername);
  REQUIRE(copy.getEmail() == originalEmail);
  REQUIRE(copy.getUserType() == originalType);
  REQUIRE(copy == original);
}
