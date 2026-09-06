/**
 * Message Class unit tests
 */

#include "utils/models/message.h"
#include "utils/models/user.h"

#include <catch2/catch_test_macros.hpp>

#include <ctime>
#include <sstream>
#include <string>

static const User alice("alice", "alice@example.com", User::UserType::USER);
static const User bob("bob", "bob@example.com", User::UserType::USER);

TEST_CASE("Message stores sender, receiver, and content", "[message][ctor]") {
  SECTION("plain text") {
    const Message msg(alice, bob, "hello");
    REQUIRE(msg.getFrom() == alice);
    REQUIRE(msg.getTo() == bob);
    REQUIRE(msg.getContent() == "hello");
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("empty content") {
    const Message msg(alice, bob, "");
    REQUIRE(msg.getFrom() == alice);
    REQUIRE(msg.getTo() == bob);
    REQUIRE(msg.getContent().empty());
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("multiline content") {
    const Message msg(alice, bob, "line1\nline2");
    REQUIRE(msg.getFrom() == alice);
    REQUIRE(msg.getTo() == bob);
    REQUIRE(msg.getContent() == "line1\nline2");
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("unicode content") {
    const Message msg(alice, bob, "unicode: \xCE\xB1\xCE\xB2\xCE\xB3");
    REQUIRE(msg.getFrom() == alice);
    REQUIRE(msg.getTo() == bob);
    REQUIRE(msg.getContent() == "unicode: \xCE\xB1\xCE\xB2\xCE\xB3");
    REQUIRE_FALSE(msg.getId().empty());
  }
}

TEST_CASE("Message timestamp is set near construction time", "[message][timestamp]") {
  const auto before = std::time(nullptr);
  const Message msg(alice, bob, "ping");
  const auto after = std::time(nullptr);

  REQUIRE(msg.getTimestamp() >= before);
  REQUIRE(msg.getTimestamp() <= after);
}

TEST_CASE("Successive messages receive unique ids", "[message][id]") {
  const Message first(alice, bob, "one");
  const Message second(alice, bob, "two");

  REQUIRE(first.getId() != second.getId());
  REQUIRE(first != second);
  REQUIRE_FALSE(first == second);
}

TEST_CASE("A message compares equal only to itself", "[message][equality]") {
  const Message msg(alice, bob, "same");

  REQUIRE(msg == msg);
  REQUIRE_FALSE(msg != msg);
  REQUIRE_FALSE(msg < msg);
  REQUIRE_FALSE(msg > msg);
  REQUIRE(msg <= msg);
  REQUIRE(msg >= msg);
}

TEST_CASE("Message stream output includes users and content", "[message][print]") {
  SECTION("plain text") {
    const Message msg(alice, bob, "hello");
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("alice") != std::string::npos);
    REQUIRE(text.find("bob") != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
    REQUIRE(text.find(msg.getId()) != std::string::npos);
  }

  SECTION("empty content") {
    const Message msg(alice, bob, "");
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("alice") != std::string::npos);
    REQUIRE(text.find("bob") != std::string::npos);
    REQUIRE(text.find(msg.getId()) != std::string::npos);
  }

  SECTION("multiline content") {
    const Message msg(alice, bob, "line1\nline2");
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("alice") != std::string::npos);
    REQUIRE(text.find("bob") != std::string::npos);
    REQUIRE(text.find("line1\nline2") != std::string::npos);
    REQUIRE(text.find(msg.getId()) != std::string::npos);
  }
}
