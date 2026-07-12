/**
 * Message test file
 *
 * @brief Test cases for the Message class
 * @date 12-07-2026
 */

#include "utils/models/message.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// capture the stream output of a message
namespace {

std::string captureStream(const Message &message) {
  std::ostringstream stream;
  stream << message;
  return stream.str();
}

uint64_t parseId(const Message &message) { return std::stoull(message.getId()); }

} // namespace

// test the constructor of the Message class
TEST_CASE("Constructor Test", "[Message][constructor]") {
  // test the INFO message
  SECTION("INFO message") {
    Message message("hello", Message::Type::INFO);
    REQUIRE(message.getMessage() == "hello");
    REQUIRE(message.getType() == Message::Type::INFO);
  }

  // test the WARNING message
  SECTION("WARNING message") {
    Message message("careful", Message::Type::WARNING);
    REQUIRE(message.getMessage() == "careful");
    REQUIRE(message.getType() == Message::Type::WARNING);
  }

  // test the ERROR message
  SECTION("ERROR message") {
    Message message("failed", Message::Type::ERROR);
    REQUIRE(message.getMessage() == "failed");
    REQUIRE(message.getType() == Message::Type::ERROR);
  }
}

// test the edge cases of the Message class
TEST_CASE("Constructor Edge Cases", "[Message][constructor][edge]") {
  // test the empty string content
  SECTION("empty string content") {
    Message message("", Message::Type::INFO);
    REQUIRE(message.getMessage().empty());
    REQUIRE(message.getType() == Message::Type::INFO);
  }

  // test the whitespace-only content
  SECTION("whitespace-only content") {
    Message message("   \t\n  ", Message::Type::WARNING);
    REQUIRE(message.getMessage() == "   \t\n  ");
  }

  // test the very long content
  SECTION("very long content") {
    const std::string longText(10'000, 'x');
    Message message(longText, Message::Type::ERROR);
    REQUIRE(message.getMessage() == longText);
    REQUIRE(message.getMessage().size() == 10'000);
  }

  // test the special characters and unicode
  SECTION("special characters and unicode") {
    const std::string special = "line1\nline2\t\"quoted\" \\ slash \xE2\x98\x83";
    Message message(special, Message::Type::INFO);
    REQUIRE(message.getMessage() == special);
  }

  // test the string_view slice without null terminator in source range
  SECTION("string_view slice without null terminator in source range") {
    const std::string source = "prefix:payload:suffix";
    Message message(std::string_view(source).substr(7, 7), Message::Type::INFO);
    REQUIRE(message.getMessage() == "payload");
  }
}

// test the metadata of the Message class
TEST_CASE("Metadata Test", "[Message][metadata]") {
  // test the timestamp is set at construction
  SECTION("timestamp is set at construction") {
    const std::time_t before = std::time(nullptr);
    Message message("metadata test", Message::Type::INFO);
    const std::time_t after = std::time(nullptr);

    REQUIRE_FALSE(message.getId().empty());
    REQUIRE(message.getTimestamp() >= before);
    REQUIRE(message.getTimestamp() <= after);
  }

  // test the id is a numeric string
  SECTION("id is a numeric string") {
    Message message("id format", Message::Type::INFO);
    REQUIRE_NOTHROW(parseId(message));
  }

  // test the successive messages receive unique increasing ids
  SECTION("successive messages receive unique increasing ids") {
    Message first("one", Message::Type::INFO);
    Message second("two", Message::Type::WARNING);
    Message third("three", Message::Type::ERROR);

    const uint64_t firstId = parseId(first);
    const uint64_t secondId = parseId(second);
    const uint64_t thirdId = parseId(third);

    REQUIRE(firstId != secondId);
    REQUIRE(secondId != thirdId);
    REQUIRE(firstId < secondId);
    REQUIRE(secondId < thirdId);
  }

  // test the rapid creation still yields unique ids
  SECTION("rapid creation still yields unique ids") {
    std::vector<Message> messages;
    messages.reserve(20);
    for (int i = 0; i < 20; ++i) {
      messages.emplace_back("msg-" + std::to_string(i), Message::Type::INFO);
    }

    for (int i = 0; i < 20; ++i) {
      for (int j = i + 1; j < 20; ++j) {
        REQUIRE(messages[i].getId() != messages[j].getId());
      }
    }
  }
}

// test the setters of the Message class
TEST_CASE("Setters Test", "[Message][setters]") {
  Message message("original", Message::Type::INFO);

  // test the setMessage with normal text
  SECTION("setMessage with normal text") {
    message.setMessage("updated");
    REQUIRE(message.getMessage() == "updated");
  }

  // test the setMessage with empty string
  SECTION("setMessage with empty string") {
    message.setMessage("");
    REQUIRE(message.getMessage().empty());
  }

  // test the setMessage with long text
  SECTION("setMessage with long text") {
    const std::string longText(5'000, 'z');
    message.setMessage(longText);
    REQUIRE(message.getMessage() == longText);
  }

  // test the setMessage with string_view slice
  SECTION("setMessage with string_view slice") {
    const std::string source = "before-after";
    message.setMessage(std::string_view(source).substr(7));
    REQUIRE(message.getMessage() == "after");
  }

  // test the setType cycles through all values
  SECTION("setType cycles through all values") {
    message.setType(Message::Type::WARNING);
    REQUIRE(message.getType() == Message::Type::WARNING);

    message.setType(Message::Type::ERROR);
    REQUIRE(message.getType() == Message::Type::ERROR);

    message.setType(Message::Type::INFO);
    REQUIRE(message.getType() == Message::Type::INFO);
  }

  // test the setTimestamp accepts boundary values
  SECTION("setTimestamp accepts boundary values") {
    message.setTimestamp(0);
    REQUIRE(message.getTimestamp() == 0);

    const std::time_t future = 4'102'444'800;
    message.setTimestamp(future);
    REQUIRE(message.getTimestamp() == future);
  }

  // test the setters do not change id
  SECTION("setters do not change id") {
    const std::string originalId = message.getId();
    message.setMessage("changed");
    message.setType(Message::Type::ERROR);
    message.setTimestamp(123);
    REQUIRE(message.getId() == originalId);
  }
}

// test the equality of the Message class
TEST_CASE("Equality Test", "[Message][equality]") {
  Message first("first text", Message::Type::INFO);
  Message second("second text", Message::Type::ERROR);

  REQUIRE_FALSE(first == second);

  // test the same object is equal to itself
  SECTION("same object is equal to itself") { REQUIRE(first == first); }

  // test the copied message shares id and is equal
  SECTION("copied message shares id and is equal") {
    const Message copy = first;
    REQUIRE(copy == first);
    REQUIRE(copy.getMessage() == first.getMessage());
    REQUIRE(copy.getType() == first.getType());
    REQUIRE(copy.getTimestamp() == first.getTimestamp());
  }

  // test the different messages always have different ids
  SECTION("different messages always have different ids") {
    Message a("a", Message::Type::INFO);
    Message b("b", Message::Type::WARNING);
    REQUIRE(a.getId() != b.getId());
    REQUIRE_FALSE(a == b);
  }

  // test the equality compares id only
  SECTION("equality compares id only") {
    Message original("alpha", Message::Type::INFO);
    Message sameId = original;

    sameId.setMessage("beta");
    sameId.setType(Message::Type::ERROR);
    sameId.setTimestamp(42);

    REQUIRE(sameId == original);
    REQUIRE(sameId.getMessage() != original.getMessage());
    REQUIRE(sameId.getType() != original.getType());
    REQUIRE(sameId.getTimestamp() != original.getTimestamp());
  }

  // test the messages with same content and type but different ids are not equal
  SECTION("messages with same content and type but different ids are not equal") {
    Message a("same", Message::Type::WARNING);
    Message b("same", Message::Type::WARNING);
    REQUIRE(a.getId() != b.getId());
    REQUIRE_FALSE(a == b);
  }
}

// test the stream output format of the Message class
TEST_CASE("Stream Output Format", "[Message][ostream]") {
  Message message("stream test", Message::Type::WARNING);
  const std::string output = captureStream(message);

  REQUIRE(output.find("[Msg: " + message.getId() + "]") != std::string::npos);
  REQUIRE(output.find("(WARNING, " + std::to_string(message.getTimestamp()) + ")") !=
          std::string::npos);
  REQUIRE(output.find("Message: stream test") != std::string::npos);
}

// test the stream output type labels of the Message class
TEST_CASE("Stream Output Type Labels", "[Message][ostream][types]") {
  // test the INFO label
  SECTION("INFO label") {
    Message message("info", Message::Type::INFO);
    REQUIRE(captureStream(message).find("(INFO, ") != std::string::npos);
  }

  // test the WARNING label
  SECTION("WARNING label") {
    Message message("warn", Message::Type::WARNING);
    REQUIRE(captureStream(message).find("(WARNING, ") != std::string::npos);
  }

  // test the ERROR label
  SECTION("ERROR label") {
    Message message("err", Message::Type::ERROR);
    REQUIRE(captureStream(message).find("(ERROR, ") != std::string::npos);
  }
}

// test the stream output edge cases of the Message class
TEST_CASE("Stream Output Edge Cases", "[Message][ostream][edge]") {
  // test the empty message content still prints label
  SECTION("empty message content still prints label") {
    Message message("", Message::Type::INFO);
    const std::string output = captureStream(message);
    REQUIRE(output.find("Message: \n") != std::string::npos);
  }

  // test the multiline content is preserved in output
  SECTION("multiline content is preserved in output") {
    Message message("line one\nline two", Message::Type::ERROR);
    const std::string output = captureStream(message);
    REQUIRE(output.find("line one\nline two") != std::string::npos);
  }

  // test the id appears in stream header
  SECTION("id appears in stream header") {
    Message message("content", Message::Type::INFO);
    const std::string output = captureStream(message);
    REQUIRE(output.find("[Msg: " + message.getId() + "]") != std::string::npos);
  }
}

// test the getters of the Message class
TEST_CASE("Getters Test", "[Message][getters]") {
  Message message("start", Message::Type::INFO);

  // test the getMessage returns the correct value
  SECTION("getMessage returns the correct value") {
    message.setMessage("end");
    REQUIRE(message.getMessage() == "end");
  }

  // test the getType returns the correct value
  SECTION("getType returns the correct value") {
    message.setType(Message::Type::ERROR);
    REQUIRE(message.getType() == Message::Type::ERROR);
  }

  // test the getTimestamp returns the correct value
  SECTION("getTimestamp returns the correct value") {
    message.setTimestamp(99);
    REQUIRE(message.getTimestamp() == 99);
  }

  // test the getters return consistent values after updates
  SECTION("getters return consistent values after updates") {
    message.setMessage("end");
    message.setType(Message::Type::ERROR);
    message.setTimestamp(42);

    REQUIRE(message.getMessage() == "end");
    REQUIRE(message.getType() == Message::Type::ERROR);
    REQUIRE(message.getTimestamp() == 42);
    REQUIRE_FALSE(message.getId().empty());
  }
}

// test the copy semantics of the Message class
TEST_CASE("Copy Semantics Test", "[Message][copy]") {
  Message original("copy me", Message::Type::WARNING);
  const std::string originalId = original.getId();
  const std::string originalMessage = original.getMessage();
  const auto originalType = original.getType();
  const std::time_t originalTimestamp = original.getTimestamp();

  Message copy = original;

  REQUIRE(copy.getId() == originalId);
  REQUIRE(copy.getMessage() == originalMessage);
  REQUIRE(copy.getType() == originalType);
  REQUIRE(copy.getTimestamp() == originalTimestamp);
  REQUIRE(copy == original);
}
