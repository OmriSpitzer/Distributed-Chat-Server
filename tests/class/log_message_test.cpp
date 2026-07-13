/**
 * Message test file
 *
 * @brief Test cases for the Message class
 * @date 12-07-2026
 */

#include "utils/models/log_message.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// capture the stream output of a message
namespace {

std::string captureStream(const LogMessage &message) {
  std::ostringstream stream;
  stream << message;
  return stream.str();
}

uint64_t parseId(const LogMessage &message) { return std::stoull(message.getId()); }

} // namespace

// test the constructor of the Message class
TEST_CASE("Constructor Test", "[LogMessage][constructor]") {
  // test the INFO message
  SECTION("INFO message") {
    LogMessage message("hello", LogMessage::Type::INFO);
    REQUIRE(message.getMessage() == "hello");
    REQUIRE(message.getType() == LogMessage::Type::INFO);
  }

  // test the WARNING message
  SECTION("WARNING message") {
    LogMessage message("careful", LogMessage::Type::WARNING);
    REQUIRE(message.getMessage() == "careful");
    REQUIRE(message.getType() == LogMessage::Type::WARNING);
  }

  // test the ERROR message
  SECTION("ERROR message") {
    LogMessage message("failed", LogMessage::Type::ERROR);
    REQUIRE(message.getMessage() == "failed");
    REQUIRE(message.getType() == LogMessage::Type::ERROR);
  }
}

// test the edge cases of the Message class
TEST_CASE("Constructor Edge Cases", "[LogMessage][constructor][edge]") {
  // test the empty string content
  SECTION("empty string content") {
    LogMessage message("", LogMessage::Type::INFO);
    REQUIRE(message.getMessage().empty());
    REQUIRE(message.getType() == LogMessage::Type::INFO);
  }

  // test the whitespace-only content
  SECTION("whitespace-only content") {
    LogMessage message("   \t\n  ", LogMessage::Type::WARNING);
    REQUIRE(message.getMessage() == "   \t\n  ");
  }

  // test the very long content
  SECTION("very long content") {
    const std::string longText(10'000, 'x');
    LogMessage message(longText, LogMessage::Type::ERROR);
    REQUIRE(message.getMessage() == longText);
    REQUIRE(message.getMessage().size() == 10'000);
  }

  // test the special characters and unicode
  SECTION("special characters and unicode") {
    const std::string special = "line1\nline2\t\"quoted\" \\ slash \xE2\x98\x83";
    LogMessage message(special, LogMessage::Type::INFO);
    REQUIRE(message.getMessage() == special);
  }

  // test the string_view slice without null terminator in source range
  SECTION("string_view slice without null terminator in source range") {
    const std::string source = "prefix:payload:suffix";
    LogMessage message(std::string_view(source).substr(7, 7), LogMessage::Type::INFO);
    REQUIRE(message.getMessage() == "payload");
  }
}

// test the metadata of the Message class
TEST_CASE("Metadata Test", "[LogMessage][metadata]") {
  // test the timestamp is set at construction
  SECTION("timestamp is set at construction") {
    const std::time_t before = std::time(nullptr);
    LogMessage message("metadata test", LogMessage::Type::INFO);
    const std::time_t after = std::time(nullptr);

    REQUIRE_FALSE(message.getId().empty());
    REQUIRE(message.getTimestamp() >= before);
    REQUIRE(message.getTimestamp() <= after);
  }

  // test the id is a numeric string
  SECTION("id is a numeric string") {
    LogMessage message("id format", LogMessage::Type::INFO);
    REQUIRE_NOTHROW(parseId(message));
  }

  // test the successive messages receive unique increasing ids
  SECTION("successive messages receive unique increasing ids") {
    LogMessage first("one", LogMessage::Type::INFO);
    LogMessage second("two", LogMessage::Type::WARNING);
    LogMessage third("three", LogMessage::Type::ERROR);

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
    std::vector<LogMessage> messages;
    messages.reserve(20);
    for (int i = 0; i < 20; ++i) {
      messages.emplace_back("msg-" + std::to_string(i), LogMessage::Type::INFO);
    }

    for (int i = 0; i < 20; ++i) {
      for (int j = i + 1; j < 20; ++j) {
        REQUIRE(messages[i].getId() != messages[j].getId());
      }
    }
  }
}

// test the setters of the Message class
TEST_CASE("Setters Test", "[LogMessage][setters]") {
  LogMessage message("original", LogMessage::Type::INFO);

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
    message.setType(LogMessage::Type::WARNING);
    REQUIRE(message.getType() == LogMessage::Type::WARNING);

    message.setType(LogMessage::Type::ERROR);
    REQUIRE(message.getType() == LogMessage::Type::ERROR);

    message.setType(LogMessage::Type::INFO);
    REQUIRE(message.getType() == LogMessage::Type::INFO);
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
    message.setType(LogMessage::Type::ERROR);
    message.setTimestamp(123);
    REQUIRE(message.getId() == originalId);
  }
}

// test the equality of the Message class
TEST_CASE("Equality Test", "[LogMessage][equality]") {
  LogMessage first("first text", LogMessage::Type::INFO);
  LogMessage second("second text", LogMessage::Type::ERROR);

  REQUIRE_FALSE(first == second);

  // test the same object is equal to itself
  SECTION("same object is equal to itself") { REQUIRE(first == first); }

  // test the copied message shares id and is equal
  SECTION("copied message shares id and is equal") {
    const LogMessage copy = first;
    REQUIRE(copy == first);
    REQUIRE(copy.getMessage() == first.getMessage());
    REQUIRE(copy.getType() == first.getType());
    REQUIRE(copy.getTimestamp() == first.getTimestamp());
  }

  // test the different messages always have different ids
  SECTION("different messages always have different ids") {
    LogMessage a("a", LogMessage::Type::INFO);
    LogMessage b("b", LogMessage::Type::WARNING);
    REQUIRE(a.getId() != b.getId());
    REQUIRE_FALSE(a == b);
  }

  // test the equality compares id only
  SECTION("equality compares id only") {
    LogMessage original("alpha", LogMessage::Type::INFO);
    LogMessage sameId = original;

    sameId.setMessage("beta");
    sameId.setType(LogMessage::Type::ERROR);
    sameId.setTimestamp(42);

    REQUIRE(sameId == original);
    REQUIRE(sameId.getMessage() != original.getMessage());
    REQUIRE(sameId.getType() != original.getType());
    REQUIRE(sameId.getTimestamp() != original.getTimestamp());
  }

  // test the messages with same content and type but different ids are not equal
  SECTION("messages with same content and type but different ids are not equal") {
    LogMessage a("same", LogMessage::Type::WARNING);
    LogMessage b("same", LogMessage::Type::WARNING);
    REQUIRE(a.getId() != b.getId());
    REQUIRE_FALSE(a == b);
  }
}

// test the stream output format of the Message class
TEST_CASE("Stream Output Format", "[LogMessage][ostream]") {
  LogMessage message("stream test", LogMessage::Type::WARNING);
  const std::string output = captureStream(message);

  REQUIRE(output.find("[Msg: " + message.getId() + "]") != std::string::npos);
  REQUIRE(output.find("(WARNING, " + std::to_string(message.getTimestamp()) + ")") !=
          std::string::npos);
  REQUIRE(output.find("Message: stream test") != std::string::npos);
}

// test the stream output type labels of the Message class
TEST_CASE("Stream Output Type Labels", "[LogMessage][ostream][types]") {
  // test the INFO label
  SECTION("INFO label") {
    LogMessage message("info", LogMessage::Type::INFO);
    REQUIRE(captureStream(message).find("(INFO, ") != std::string::npos);
  }

  // test the WARNING label
  SECTION("WARNING label") {
    LogMessage message("warn", LogMessage::Type::WARNING);
    REQUIRE(captureStream(message).find("(WARNING, ") != std::string::npos);
  }

  // test the ERROR label
  SECTION("ERROR label") {
    LogMessage message("err", LogMessage::Type::ERROR);
    REQUIRE(captureStream(message).find("(ERROR, ") != std::string::npos);
  }
}

// test the stream output edge cases of the Message class
TEST_CASE("Stream Output Edge Cases", "[LogMessage][ostream][edge]") {
  // test the empty message content still prints label
  SECTION("empty message content still prints label") {
    LogMessage message("", LogMessage::Type::INFO);
    const std::string output = captureStream(message);
    REQUIRE(output.find("Message: \n") != std::string::npos);
  }

  // test the multiline content is preserved in output
  SECTION("multiline content is preserved in output") {
    LogMessage message("line one\nline two", LogMessage::Type::ERROR);
    const std::string output = captureStream(message);
    REQUIRE(output.find("line one\nline two") != std::string::npos);
  }

  // test the id appears in stream header
  SECTION("id appears in stream header") {
    LogMessage message("content", LogMessage::Type::INFO);
    const std::string output = captureStream(message);
    REQUIRE(output.find("[Msg: " + message.getId() + "]") != std::string::npos);
  }
}

// test the getters of the Message class
TEST_CASE("Getters Test", "[LogMessage][getters]") {
  LogMessage message("start", LogMessage::Type::INFO);

  // test the getMessage returns the correct value
  SECTION("getMessage returns the correct value") {
    message.setMessage("end");
    REQUIRE(message.getMessage() == "end");
  }

  // test the getType returns the correct value
  SECTION("getType returns the correct value") {
    message.setType(LogMessage::Type::ERROR);
    REQUIRE(message.getType() == LogMessage::Type::ERROR);
  }

  // test the getTimestamp returns the correct value
  SECTION("getTimestamp returns the correct value") {
    message.setTimestamp(99);
    REQUIRE(message.getTimestamp() == 99);
  }

  // test the getters return consistent values after updates
  SECTION("getters return consistent values after updates") {
    message.setMessage("end");
    message.setType(LogMessage::Type::ERROR);
    message.setTimestamp(42);

    REQUIRE(message.getMessage() == "end");
    REQUIRE(message.getType() == LogMessage::Type::ERROR);
    REQUIRE(message.getTimestamp() == 42);
    REQUIRE_FALSE(message.getId().empty());
  }
}

// test the copy semantics of the Message class
TEST_CASE("Copy Semantics Test", "[LogMessage][copy]") {
  LogMessage original("copy me", LogMessage::Type::WARNING);
  const std::string originalId = original.getId();
  const std::string originalMessage = original.getMessage();
  const auto originalType = original.getType();
  const std::time_t originalTimestamp = original.getTimestamp();

  LogMessage copy = original;

  REQUIRE(copy.getId() == originalId);
  REQUIRE(copy.getMessage() == originalMessage);
  REQUIRE(copy.getType() == originalType);
  REQUIRE(copy.getTimestamp() == originalTimestamp);
  REQUIRE(copy == original);
}
