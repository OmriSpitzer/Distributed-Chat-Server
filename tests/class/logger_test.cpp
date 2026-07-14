/**
 * Logger test file
 *
 * @brief Test cases for the Logger class
 * @date 12-07-2026
 */

#include "utils/models/logger.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <ctime>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string captureStream() {
  std::ostringstream stream;
  stream << Logger::getInstance();
  return stream.str();
}

uint64_t parseId(const LogMessage &message) { return std::stoull(message.getId()); }

} // namespace

// test the empty singleton logger
TEST_CASE("Empty Logger Test", "[Logger][constructor]") {
  Logger::clear();
  const std::string output = captureStream();

  REQUIRE(output.find("Logger: 0 messages") != std::string::npos);
  REQUIRE(output.find("Messages:") != std::string::npos);
  REQUIRE_THROWS_AS(Logger::getMessage(0), std::out_of_range);
}

// test the log methods of the Logger class
TEST_CASE("Log Methods Test", "[Logger][logging]") {
  SECTION("logInfo stores INFO messages") {
    Logger::clear();
    Logger::logInfo("info message");

    const LogMessage message = Logger::getMessage(0);
    REQUIRE(message.getMessage() == "info message");
    REQUIRE(message.getType() == LogMessage::Type::INFO);
  }

  SECTION("logWarning stores WARNING messages") {
    Logger::clear();
    Logger::logWarning("warning message");

    const LogMessage message = Logger::getMessage(0);
    REQUIRE(message.getMessage() == "warning message");
    REQUIRE(message.getType() == LogMessage::Type::WARNING);
  }

  SECTION("logError stores ERROR messages") {
    Logger::clear();
    Logger::logError("error message");

    const LogMessage message = Logger::getMessage(0);
    REQUIRE(message.getMessage() == "error message");
    REQUIRE(message.getType() == LogMessage::Type::ERROR);
  }
}

// test the message ordering of the Logger class
TEST_CASE("Ordering Test", "[Logger][ordering]") {
  Logger::clear();
  Logger::logInfo("first");
  Logger::logWarning("second");
  Logger::logError("third");

  SECTION("content order is preserved") {
    REQUIRE(Logger::getMessage(0).getMessage() == "first");
    REQUIRE(Logger::getMessage(1).getMessage() == "second");
    REQUIRE(Logger::getMessage(2).getMessage() == "third");
  }

  SECTION("type order is preserved") {
    REQUIRE(Logger::getMessage(0).getType() == LogMessage::Type::INFO);
    REQUIRE(Logger::getMessage(1).getType() == LogMessage::Type::WARNING);
    REQUIRE(Logger::getMessage(2).getType() == LogMessage::Type::ERROR);
  }
}

// test mixed message types in the Logger class
TEST_CASE("Mixed Types Test", "[Logger][types]") {
  Logger::clear();
  Logger::logInfo("info");
  Logger::logWarning("warn");
  Logger::logError("err");
  Logger::logInfo("info again");

  REQUIRE(Logger::getMessage(0).getType() == LogMessage::Type::INFO);
  REQUIRE(Logger::getMessage(1).getType() == LogMessage::Type::WARNING);
  REQUIRE(Logger::getMessage(2).getType() == LogMessage::Type::ERROR);
  REQUIRE(Logger::getMessage(3).getType() == LogMessage::Type::INFO);
}

// test getMessage edge cases of the Logger class
TEST_CASE("GetMessage Edge Cases", "[Logger][getMessage][edge]") {
  Logger::clear();
  Logger::logInfo("only");

  SECTION("valid first index") { REQUIRE(Logger::getMessage(0).getMessage() == "only"); }

  SECTION("valid last index in multi-message logger") {
    Logger::logWarning("last");
    REQUIRE(Logger::getMessage(1).getMessage() == "last");
  }

  SECTION("negative index throws") { REQUIRE_THROWS_AS(Logger::getMessage(-1), std::out_of_range); }

  SECTION("out of bounds index throws") {
    REQUIRE_THROWS_AS(Logger::getMessage(1), std::out_of_range);
    REQUIRE_THROWS_AS(Logger::getMessage(99), std::out_of_range);
  }
}

// test unusual message content accepted by the Logger class
TEST_CASE("Content Edge Cases", "[Logger][content][edge]") {
  SECTION("empty string") {
    Logger::clear();
    Logger::logInfo("");
    REQUIRE(Logger::getMessage(0).getMessage().empty());
  }

  SECTION("whitespace-only string") {
    Logger::clear();
    Logger::logWarning("   \t\n");
    REQUIRE(Logger::getMessage(0).getMessage() == "   \t\n");
  }

  SECTION("very long string") {
    Logger::clear();
    const std::string longText(8'000, 'L');
    Logger::logError(longText);
    REQUIRE(Logger::getMessage(0).getMessage() == longText);
  }

  SECTION("special characters") {
    Logger::clear();
    const std::string special = "quote \" backslash \\ newline\n tab\t";
    Logger::logInfo(special);
    REQUIRE(Logger::getMessage(0).getMessage() == special);
  }
}

// test string_view support in the Logger class
TEST_CASE("String View Test", "[Logger][string_view]") {
  const std::string source = "prefix:payload:suffix";

  SECTION("logInfo accepts string_view slice") {
    Logger::clear();
    Logger::logInfo(std::string_view(source).substr(7, 7));
    REQUIRE(Logger::getMessage(0).getMessage() == "payload");
  }

  SECTION("logWarning accepts string_view slice") {
    Logger::clear();
    Logger::logWarning(std::string_view(source).substr(7, 7));
    REQUIRE(Logger::getMessage(0).getMessage() == "payload");
  }

  SECTION("logError accepts string_view slice") {
    Logger::clear();
    Logger::logError(std::string_view(source).substr(7, 7));
    REQUIRE(Logger::getMessage(0).getMessage() == "payload");
  }
}

// test metadata of messages stored by the Logger class
TEST_CASE("Stored Message Metadata Test", "[Logger][metadata]") {
  SECTION("logged messages receive ids and timestamps") {
    Logger::clear();
    const std::time_t before = std::time(nullptr);
    Logger::logInfo("metadata");
    const std::time_t after = std::time(nullptr);

    const LogMessage message = Logger::getMessage(0);
    REQUIRE_FALSE(message.getId().empty());
    REQUIRE_NOTHROW(parseId(message));
    REQUIRE(message.getTimestamp() >= before);
    REQUIRE(message.getTimestamp() <= after);
  }

  SECTION("logged messages receive unique increasing ids") {
    Logger::clear();
    Logger::logInfo("one");
    Logger::logWarning("two");
    Logger::logError("three");

    const uint64_t firstId = parseId(Logger::getMessage(0));
    const uint64_t secondId = parseId(Logger::getMessage(1));
    const uint64_t thirdId = parseId(Logger::getMessage(2));

    REQUIRE(firstId < secondId);
    REQUIRE(secondId < thirdId);
  }
}

// test the stream output of the Logger class
TEST_CASE("Stream Output Test", "[Logger][ostream]") {
  Logger::clear();
  Logger::logInfo("one");
  Logger::logWarning("two");

  const std::string output = captureStream();

  REQUIRE(output.find("Logger: 2 messages") != std::string::npos);
  REQUIRE(output.find("Messages:") != std::string::npos);
  REQUIRE(output.find("one") != std::string::npos);
  REQUIRE(output.find("two") != std::string::npos);
  REQUIRE(output.find("WARNING") != std::string::npos);
}

// test stream output type labels in the Logger class
TEST_CASE("Stream Output Type Labels", "[Logger][ostream][types]") {
  SECTION("INFO label appears in output") {
    Logger::clear();
    Logger::logInfo("info");
    REQUIRE(captureStream().find("(INFO, ") != std::string::npos);
  }

  SECTION("WARNING label appears in output") {
    Logger::clear();
    Logger::logWarning("warn");
    REQUIRE(captureStream().find("(WARNING, ") != std::string::npos);
  }

  SECTION("ERROR label appears in output") {
    Logger::clear();
    Logger::logError("err");
    REQUIRE(captureStream().find("(ERROR, ") != std::string::npos);
  }
}

// test stream output edge cases of the Logger class
TEST_CASE("Stream Output Edge Cases", "[Logger][ostream][edge]") {
  SECTION("empty logger prints zero messages") {
    Logger::clear();
    const std::string output = captureStream();
    REQUIRE(output.find("Logger: 0 messages") != std::string::npos);
    REQUIRE(output.find("Messages:") != std::string::npos);
  }

  SECTION("many messages are all included in output") {
    Logger::clear();
    for (int i = 0; i < 5; ++i) {
      Logger::logInfo("msg-" + std::to_string(i));
    }

    const std::string output = captureStream();
    REQUIRE(output.find("Logger: 5 messages") != std::string::npos);
    for (int i = 0; i < 5; ++i) {
      REQUIRE(output.find("msg-" + std::to_string(i)) != std::string::npos);
    }
  }

  SECTION("stored message ids appear in output") {
    Logger::clear();
    Logger::logInfo("tracked");
    const LogMessage stored = Logger::getMessage(0);
    REQUIRE(captureStream().find("[Msg: " + stored.getId() + "]") != std::string::npos);
  }
}

// test independent message copies returned by getMessage
TEST_CASE("Independent Copies Test", "[Logger][copies]") {
  Logger::clear();
  Logger::logInfo("original");

  SECTION("mutating retrieved message content does not affect logger") {
    LogMessage retrieved = Logger::getMessage(0);
    retrieved.setMessage("mutated");

    REQUIRE(Logger::getMessage(0).getMessage() == "original");
    REQUIRE(retrieved.getMessage() == "mutated");
  }

  SECTION("mutating retrieved message type does not affect logger") {
    LogMessage retrieved = Logger::getMessage(0);
    retrieved.setType(LogMessage::Type::ERROR);

    REQUIRE(Logger::getMessage(0).getType() == LogMessage::Type::INFO);
    REQUIRE(retrieved.getType() == LogMessage::Type::ERROR);
  }

  SECTION("mutating retrieved message timestamp does not affect logger") {
    LogMessage retrieved = Logger::getMessage(0);
    retrieved.setTimestamp(123);

    REQUIRE(Logger::getMessage(0).getTimestamp() != 123);
    REQUIRE(retrieved.getTimestamp() == 123);
  }
}

// test repeated logging calls in the Logger class
TEST_CASE("Repeated Logging Test", "[Logger][stress]") {
  Logger::clear();

  for (int i = 0; i < 100; ++i) {
    Logger::logInfo("entry-" + std::to_string(i));
  }

  REQUIRE(Logger::getMessage(0).getMessage() == "entry-0");
  REQUIRE(Logger::getMessage(50).getMessage() == "entry-50");
  REQUIRE(Logger::getMessage(99).getMessage() == "entry-99");
  REQUIRE_THROWS_AS(Logger::getMessage(100), std::out_of_range);

  SECTION("stream output reflects full count") {
    REQUIRE(captureStream().find("Logger: 100 messages") != std::string::npos);
  }
}
