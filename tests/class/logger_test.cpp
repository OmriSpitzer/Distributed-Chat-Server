/**
 * Logger test file
 *
 * @brief Test cases for the Logger class
 * @date 12-07-2026
 */

#include "utils/logger.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

std::string captureStream(const Logger &logger) {
  std::ostringstream stream;
  stream << logger;
  return stream.str();
}

uint64_t parseId(const Message &message) { return std::stoull(message.getId()); }

} // namespace

// test the constructor of the Logger class
TEST_CASE("Constructor Test", "[Logger][constructor]") {
  Logger logger;
  const std::string output = captureStream(logger);

  // test the output contains the correct number of messages
  REQUIRE(output.find("Logger: 0 messages") != std::string::npos);

  // test the output contains the correct number of messages
  REQUIRE(output.find("Logger: 0 messages") != std::string::npos);

  // test the output contains the correct number of messages
  REQUIRE(output.find("Messages:") != std::string::npos);

  // test the getMessage throws an out of range exception
  REQUIRE_THROWS_AS(logger.getMessage(0), std::out_of_range);
}

// test the log methods of the Logger class
TEST_CASE("Log Methods Test", "[Logger][logging]") {
  // test the logInfo stores INFO messages
  SECTION("logInfo stores INFO messages") {
    Logger logger;
    logger.logInfo("info message");

    const Message message = logger.getMessage(0);
    REQUIRE(message.getMessage() == "info message");
    REQUIRE(message.getType() == Message::Type::INFO);
  }

  // test the logWarning stores WARNING messages
  SECTION("logWarning stores WARNING messages") {
    Logger logger;
    logger.logWarning("warning message");

    const Message message = logger.getMessage(0);
    REQUIRE(message.getMessage() == "warning message");
    REQUIRE(message.getType() == Message::Type::WARNING);
  }

  // test the logError stores ERROR messages
  SECTION("logError stores ERROR messages") {
    Logger logger;
    logger.logError("error message");

    const Message message = logger.getMessage(0);
    REQUIRE(message.getMessage() == "error message");
    REQUIRE(message.getType() == Message::Type::ERROR);
  }
}

// test the message ordering of the Logger class
TEST_CASE("Ordering Test", "[Logger][ordering]") {
  Logger logger;
  logger.logInfo("first");
  logger.logWarning("second");
  logger.logError("third");

  // test the content order is preserved
  SECTION("content order is preserved") {
    REQUIRE(logger.getMessage(0).getMessage() == "first");
    REQUIRE(logger.getMessage(1).getMessage() == "second");
    REQUIRE(logger.getMessage(2).getMessage() == "third");
  }

  // test the type order is preserved
  SECTION("type order is preserved") {
    REQUIRE(logger.getMessage(0).getType() == Message::Type::INFO);
    REQUIRE(logger.getMessage(1).getType() == Message::Type::WARNING);
    REQUIRE(logger.getMessage(2).getType() == Message::Type::ERROR);
  }
}

// test mixed message types in the Logger class
TEST_CASE("Mixed Types Test", "[Logger][types]") {
  Logger logger;
  logger.logInfo("info");
  logger.logWarning("warn");
  logger.logError("err");
  logger.logInfo("info again");

  REQUIRE(logger.getMessage(0).getType() == Message::Type::INFO);
  REQUIRE(logger.getMessage(1).getType() == Message::Type::WARNING);
  REQUIRE(logger.getMessage(2).getType() == Message::Type::ERROR);
  REQUIRE(logger.getMessage(3).getType() == Message::Type::INFO);
}

// test getMessage edge cases of the Logger class
TEST_CASE("GetMessage Edge Cases", "[Logger][getMessage][edge]") {
  Logger logger;
  logger.logInfo("only");

  // test the valid first index
  SECTION("valid first index") { REQUIRE(logger.getMessage(0).getMessage() == "only"); }

  // test the valid last index in multi-message logger
  SECTION("valid last index in multi-message logger") {
    logger.logWarning("last");
    REQUIRE(logger.getMessage(1).getMessage() == "last");
  }

  // test the negative index throws
  SECTION("negative index throws") { REQUIRE_THROWS_AS(logger.getMessage(-1), std::out_of_range); }

  // test the out of bounds index throws
  SECTION("out of bounds index throws") {
    REQUIRE_THROWS_AS(logger.getMessage(1), std::out_of_range);
    REQUIRE_THROWS_AS(logger.getMessage(99), std::out_of_range);
  }
}

// test unusual message content accepted by the Logger class
TEST_CASE("Content Edge Cases", "[Logger][content][edge]") {
  Logger logger;

  // test the empty string
  SECTION("empty string") {
    logger.logInfo("");
    REQUIRE(logger.getMessage(0).getMessage().empty());
  }

  // test the whitespace-only string
  SECTION("whitespace-only string") {
    logger.logWarning("   \t\n");
    REQUIRE(logger.getMessage(0).getMessage() == "   \t\n");
  }

  // test the very long string
  SECTION("very long string") {
    const std::string longText(8'000, 'L');
    logger.logError(longText);
    REQUIRE(logger.getMessage(0).getMessage() == longText);
  }

  // test the special characters
  SECTION("special characters") {
    const std::string special = "quote \" backslash \\ newline\n tab\t";
    logger.logInfo(special);
    REQUIRE(logger.getMessage(0).getMessage() == special);
  }
}

// test string_view support in the Logger class
TEST_CASE("String View Test", "[Logger][string_view]") {
  const std::string source = "prefix:payload:suffix";

  // test the logInfo accepts string_view slice
  SECTION("logInfo accepts string_view slice") {
    Logger logger;
    logger.logInfo(std::string_view(source).substr(7, 7));
    REQUIRE(logger.getMessage(0).getMessage() == "payload");
  }

  // test the logWarning accepts string_view slice
  SECTION("logWarning accepts string_view slice") {
    Logger logger;
    logger.logWarning(std::string_view(source).substr(7, 7));
    REQUIRE(logger.getMessage(0).getMessage() == "payload");
  }

  // test the logError accepts string_view slice
  SECTION("logError accepts string_view slice") {
    Logger logger;
    logger.logError(std::string_view(source).substr(7, 7));
    REQUIRE(logger.getMessage(0).getMessage() == "payload");
  }
}

// test metadata of messages stored by the Logger class
TEST_CASE("Stored Message Metadata Test", "[Logger][metadata]") {
  Logger logger;

  // test the logged messages receive ids and timestamps
  SECTION("logged messages receive ids and timestamps") {
    const std::time_t before = std::time(nullptr);
    logger.logInfo("metadata");
    const std::time_t after = std::time(nullptr);

    const Message message = logger.getMessage(0);
    REQUIRE_FALSE(message.getId().empty());
    REQUIRE_NOTHROW(parseId(message));
    REQUIRE(message.getTimestamp() >= before);
    REQUIRE(message.getTimestamp() <= after);
  }

  // test the logged messages receive unique increasing ids
  SECTION("logged messages receive unique increasing ids") {
    logger.logInfo("one");
    logger.logWarning("two");
    logger.logError("three");

    const uint64_t firstId = parseId(logger.getMessage(0));
    const uint64_t secondId = parseId(logger.getMessage(1));
    const uint64_t thirdId = parseId(logger.getMessage(2));

    REQUIRE(firstId < secondId);
    REQUIRE(secondId < thirdId);
  }
}

// test the stream output of the Logger class
TEST_CASE("Stream Output Test", "[Logger][ostream]") {
  Logger logger;
  logger.logInfo("one");
  logger.logWarning("two");

  const std::string output = captureStream(logger);

  REQUIRE(output.find("Logger: 2 messages") != std::string::npos);
  REQUIRE(output.find("Messages:") != std::string::npos);
  REQUIRE(output.find("one") != std::string::npos);
  REQUIRE(output.find("two") != std::string::npos);
  REQUIRE(output.find("WARNING") != std::string::npos);
}

// test stream output type labels in the Logger class
TEST_CASE("Stream Output Type Labels", "[Logger][ostream][types]") {
  // test the INFO label appears in output
  SECTION("INFO label appears in output") {
    Logger logger;
    logger.logInfo("info");
    REQUIRE(captureStream(logger).find("(INFO, ") != std::string::npos);
  }

  // test the WARNING label appears in output
  SECTION("WARNING label appears in output") {
    Logger logger;
    logger.logWarning("warn");
    REQUIRE(captureStream(logger).find("(WARNING, ") != std::string::npos);
  }

  // test the ERROR label appears in output
  SECTION("ERROR label appears in output") {
    Logger logger;
    logger.logError("err");
    REQUIRE(captureStream(logger).find("(ERROR, ") != std::string::npos);
  }
}

// test stream output edge cases of the Logger class
TEST_CASE("Stream Output Edge Cases", "[Logger][ostream][edge]") {
  // test the empty logger prints zero messages
  SECTION("empty logger prints zero messages") {
    Logger logger;
    const std::string output = captureStream(logger);
    REQUIRE(output.find("Logger: 0 messages") != std::string::npos);
    REQUIRE(output.find("Messages:") != std::string::npos);
  }

  // test the many messages are all included in output
  SECTION("many messages are all included in output") {
    Logger logger;
    for (int i = 0; i < 5; ++i) {
      logger.logInfo("msg-" + std::to_string(i));
    }

    const std::string output = captureStream(logger);
    REQUIRE(output.find("Logger: 5 messages") != std::string::npos);
    for (int i = 0; i < 5; ++i) {
      REQUIRE(output.find("msg-" + std::to_string(i)) != std::string::npos);
    }
  }

  // test the stored message ids appear in output
  SECTION("stored message ids appear in output") {
    Logger logger;
    logger.logInfo("tracked");
    const Message stored = logger.getMessage(0);
    REQUIRE(captureStream(logger).find("[Msg: " + stored.getId() + "]") != std::string::npos);
  }
}

// test independent message copies returned by getMessage
TEST_CASE("Independent Copies Test", "[Logger][copies]") {
  Logger logger;
  logger.logInfo("original");

  // test the mutating retrieved message content does not affect logger
  SECTION("mutating retrieved message content does not affect logger") {
    Message retrieved = logger.getMessage(0);
    retrieved.setMessage("mutated");

    REQUIRE(logger.getMessage(0).getMessage() == "original");
    REQUIRE(retrieved.getMessage() == "mutated");
  }

  // test the mutating retrieved message type does not affect logger
  SECTION("mutating retrieved message type does not affect logger") {
    Message retrieved = logger.getMessage(0);
    retrieved.setType(Message::Type::ERROR);

    REQUIRE(logger.getMessage(0).getType() == Message::Type::INFO);
    REQUIRE(retrieved.getType() == Message::Type::ERROR);
  }

  // test the mutating retrieved message timestamp does not affect logger
  SECTION("mutating retrieved message timestamp does not affect logger") {
    Message retrieved = logger.getMessage(0);
    retrieved.setTimestamp(123);

    REQUIRE(logger.getMessage(0).getTimestamp() != 123);
    REQUIRE(retrieved.getTimestamp() == 123);
  }
}

// test repeated logging calls in the Logger class
TEST_CASE("Repeated Logging Test", "[Logger][stress]") {
  Logger logger;

  for (int i = 0; i < 100; ++i) {
    logger.logInfo("entry-" + std::to_string(i));
  }

  REQUIRE(logger.getMessage(0).getMessage() == "entry-0");
  REQUIRE(logger.getMessage(50).getMessage() == "entry-50");
  REQUIRE(logger.getMessage(99).getMessage() == "entry-99");
  REQUIRE_THROWS_AS(logger.getMessage(100), std::out_of_range);

  // test the stream output reflects full count
  SECTION("stream output reflects full count") {
    REQUIRE(captureStream(logger).find("Logger: 100 messages") != std::string::npos);
  }
}

// test the logger lifetime safety
TEST_CASE("Lifetime Test", "[Logger][lifetime]") {
  auto logger = std::make_unique<Logger>();
  logger->logInfo("before destroy");
  logger->logWarning("still alive");
  logger.reset();
  SUCCEED("Logger destroyed without crashing");
}
