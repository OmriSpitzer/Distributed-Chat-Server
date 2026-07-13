/**
 * Message test file
 *
 * @brief Test cases for the Message class
 * @date 12-07-2026
 */

#include "utils/models/message.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

namespace {

std::string captureStream(const Message &message) {
  std::ostringstream stream;
  stream << message;
  return stream.str();
}

} // namespace

TEST_CASE("Message constructor and stream output", "[Message]") {
  User from("alice", "alice@example.com", User::UserType::USER);
  User to("bob", "bob@example.com", User::UserType::USER);
  Message message(from, to, "hello");

  const std::string output = captureStream(message);
  REQUIRE(output.find("Message from alice to bob") != std::string::npos);
  REQUIRE(output.back() == '\n');
}

TEST_CASE("Message equality", "[Message]") {
  User from("alice", "alice@example.com", User::UserType::USER);
  User to("bob", "bob@example.com", User::UserType::USER);

  Message first(from, to, "one");
  Message second(from, to, "two");
  const Message copy = first;

  REQUIRE(first == first);
  REQUIRE(copy == first);
  REQUIRE_FALSE(first == second);
  REQUIRE(first != second);
}

TEST_CASE("Message comparison", "[Message]") {
  User from("alice", "alice@example.com", User::UserType::USER);
  User to("bob", "bob@example.com", User::UserType::USER);
  Message message(from, to, "test");

  REQUIRE(message <= message);
  REQUIRE(message >= message);
  REQUIRE_FALSE(message < message);
  REQUIRE_FALSE(message > message);
}
