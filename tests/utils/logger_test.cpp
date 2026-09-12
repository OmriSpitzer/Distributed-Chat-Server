/**
 * Logger Class unit tests
 *
 * @brief Includes: empty state, log types, source/message edges, ordering,
 * getMessage bounds, clear, capacity eviction, stream output, singleton,
 * heartbeat unique ids, mixed types, size tracking.
 * @date 12-09-2026
 */

#include "utils/models/logger.h"
#include "utils/models/log_message.h"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

/**
 * 1. empty state after clear
 * 2. logInfo / logWarning / logError / logHeartbeat
 * 3. source and message edge cases
 * 4. messages keep insertion order
 * 5. getMessage bounds
 * 6. clear removes all messages
 * 7. capacity eviction at kMaxMessages
 * 8. stream output
 * 9. singleton identity
 * 10. heartbeat messages get unique ids
 * 11. mixed types
 * 12. size tracking
 */

// minimal globals
static Logger &kLogger = Logger::getInstance();
static const char *kSrc = "logger_test";

static void resetLogger() {
  Logger::clear();
  REQUIRE(Logger::size() == 0);
}

// 1. empty state after clear
TEST_CASE("Logger is empty after clear", "[logger][empty]") {
  resetLogger();

  REQUIRE(Logger::size() == 0);
  REQUIRE_THROWS_AS(kLogger.getMessage(0), std::out_of_range);

  std::ostringstream out;
  out << kLogger;
  const std::string text = out.str();
  REQUIRE(text.find("Logger: 0 messages") != std::string::npos);
}

// 2. logInfo / logWarning / logError / logHeartbeat
TEST_CASE("Logger stores each log type", "[logger][types]") {
  resetLogger();

  SECTION("logInfo") {
    Logger::logInfo(kSrc, "hello");
    REQUIRE(Logger::size() == 1);
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getType() == LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == kSrc);
    REQUIRE(msg.getMessage() == "hello");
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("logWarning") {
    Logger::logWarning(kSrc, "watch out");
    REQUIRE(Logger::size() == 1);
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getType() == LogMessage::Type::WARNING);
    REQUIRE(msg.getSource() == kSrc);
    REQUIRE(msg.getMessage() == "watch out");
  }

  SECTION("logError") {
    Logger::logError(kSrc, "failed");
    REQUIRE(Logger::size() == 1);
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getType() == LogMessage::Type::ERROR);
    REQUIRE(msg.getSource() == kSrc);
    REQUIRE(msg.getMessage() == "failed");
  }

  SECTION("logHeartbeat") {
    Logger::logHeartbeat(kSrc, "ping");
    REQUIRE(Logger::size() == 1);
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getType() == LogMessage::Type::HEARTBEAT);
    REQUIRE(msg.getSource() == kSrc);
    REQUIRE(msg.getMessage() == "ping");
    REQUIRE_FALSE(msg.getId().empty());
  }
}

// 3. source and message edge cases
TEST_CASE("Logger source and message edge cases", "[logger][edge]") {
  resetLogger();

  SECTION("empty source") {
    Logger::logInfo("", "body");
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getSource().empty());
    REQUIRE(msg.getMessage() == "body");
  }

  SECTION("empty message") {
    Logger::logWarning(kSrc, "");
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getSource() == kSrc);
    REQUIRE(msg.getMessage().empty());
  }

  SECTION("both empty") {
    Logger::logHeartbeat("", "");
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getSource().empty());
    REQUIRE(msg.getMessage().empty());
    REQUIRE(msg.getType() == LogMessage::Type::HEARTBEAT);
  }

  SECTION("whitespace preserved") {
    Logger::logInfo("  src  ", "  msg  ");
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getSource() == "  src  ");
    REQUIRE(msg.getMessage() == "  msg  ");
  }

  SECTION("multiline message") {
    Logger::logError(kSrc, "line1\nline2\r\nline3");
    REQUIRE(kLogger.getMessage(0).getMessage() == "line1\nline2\r\nline3");
  }

  SECTION("unicode") {
    Logger::logInfo("עֹמְרִי", "שלום");
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getSource() == "עֹמְרִי");
    REQUIRE(msg.getMessage() == "שלום");
  }

  SECTION("special characters") {
    const std::string special = "a|b{c}\"d'\\e<>&;%@#";
    Logger::logWarning(special, special);
    const LogMessage msg = kLogger.getMessage(0);
    REQUIRE(msg.getSource() == special);
    REQUIRE(msg.getMessage() == special);
  }

  SECTION("embedded null byte") {
    const std::string withNull("hello\0world", 11);
    Logger::logInfo(kSrc, withNull);
    REQUIRE(kLogger.getMessage(0).getMessage() == withNull);
    REQUIRE(kLogger.getMessage(0).getMessage().size() == 11);
  }

  SECTION("long message") {
    const std::string longContent(10000, 'x');
    Logger::logInfo(kSrc, longContent);
    REQUIRE(kLogger.getMessage(0).getMessage() == longContent);
    REQUIRE(kLogger.getMessage(0).getMessage().size() == 10000);
  }
}

// 4. messages keep insertion order
TEST_CASE("Logger keeps insertion order", "[logger][order]") {
  resetLogger();

  Logger::logInfo(kSrc, "first");
  Logger::logWarning(kSrc, "second");
  Logger::logError(kSrc, "third");
  Logger::logHeartbeat(kSrc, "fourth");

  REQUIRE(Logger::size() == 4);
  REQUIRE(kLogger.getMessage(0).getMessage() == "first");
  REQUIRE(kLogger.getMessage(0).getType() == LogMessage::Type::INFO);
  REQUIRE(kLogger.getMessage(1).getMessage() == "second");
  REQUIRE(kLogger.getMessage(1).getType() == LogMessage::Type::WARNING);
  REQUIRE(kLogger.getMessage(2).getMessage() == "third");
  REQUIRE(kLogger.getMessage(2).getType() == LogMessage::Type::ERROR);
  REQUIRE(kLogger.getMessage(3).getMessage() == "fourth");
  REQUIRE(kLogger.getMessage(3).getType() == LogMessage::Type::HEARTBEAT);
}

// 5. getMessage bounds
TEST_CASE("Logger getMessage bounds", "[logger][getMessage]") {
  resetLogger();

  SECTION("empty logger rejects any index") {
    REQUIRE_THROWS_AS(kLogger.getMessage(0), std::out_of_range);
    REQUIRE_THROWS_AS(kLogger.getMessage(1), std::out_of_range);
    REQUIRE_THROWS_AS(kLogger.getMessage(Logger::kMaxMessages), std::out_of_range);
  }

  SECTION("valid indices after logging") {
    Logger::logInfo(kSrc, "a");
    Logger::logInfo(kSrc, "b");
    REQUIRE_NOTHROW(kLogger.getMessage(0));
    REQUIRE_NOTHROW(kLogger.getMessage(1));
    REQUIRE(kLogger.getMessage(0).getMessage() == "a");
    REQUIRE(kLogger.getMessage(1).getMessage() == "b");
  }

  SECTION("index equal to size is out of range") {
    Logger::logInfo(kSrc, "only");
    REQUIRE(Logger::size() == 1);
    REQUIRE_THROWS_AS(kLogger.getMessage(1), std::out_of_range);
    REQUIRE_THROWS_AS(kLogger.getMessage(100), std::out_of_range);
  }

  SECTION("getMessage returns a copy") {
    Logger::logInfo(kSrc, "original");
    LogMessage copy = kLogger.getMessage(0);
    REQUIRE(copy.getMessage() == "original");
    REQUIRE(copy == kLogger.getMessage(0));
  }
}

// 6. clear removes all messages
TEST_CASE("Logger clear removes all messages", "[logger][clear]") {
  resetLogger();

  Logger::logInfo(kSrc, "a");
  Logger::logWarning(kSrc, "b");
  Logger::logError(kSrc, "c");
  REQUIRE(Logger::size() == 3);

  Logger::clear();
  REQUIRE(Logger::size() == 0);
  REQUIRE_THROWS_AS(kLogger.getMessage(0), std::out_of_range);

  Logger::logInfo(kSrc, "after clear");
  REQUIRE(Logger::size() == 1);
  REQUIRE(kLogger.getMessage(0).getMessage() == "after clear");
}

// 7. capacity eviction at kMaxMessages
TEST_CASE("Logger evicts oldest messages past kMaxMessages", "[logger][capacity]") {
  resetLogger();

  SECTION("fills exactly to capacity") {
    for (std::size_t i = 0; i < Logger::kMaxMessages; ++i) {
      Logger::logInfo(kSrc, std::to_string(i));
    }
    REQUIRE(Logger::size() == Logger::kMaxMessages);
    REQUIRE(kLogger.getMessage(0).getMessage() == "0");
    REQUIRE(kLogger.getMessage(Logger::kMaxMessages - 1).getMessage() ==
            std::to_string(Logger::kMaxMessages - 1));
  }

  SECTION("one past capacity drops the oldest") {
    for (std::size_t i = 0; i < Logger::kMaxMessages; ++i) {
      Logger::logInfo(kSrc, std::to_string(i));
    }
    Logger::logInfo(kSrc, "overflow");

    REQUIRE(Logger::size() == Logger::kMaxMessages);
    REQUIRE(kLogger.getMessage(0).getMessage() == "1");
    REQUIRE(kLogger.getMessage(Logger::kMaxMessages - 1).getMessage() == "overflow");
  }

  SECTION("many past capacity keeps only the newest window") {
    const std::size_t extra = 50;
    for (std::size_t i = 0; i < Logger::kMaxMessages + extra; ++i) {
      Logger::logWarning(kSrc, std::to_string(i));
    }

    REQUIRE(Logger::size() == Logger::kMaxMessages);
    REQUIRE(kLogger.getMessage(0).getMessage() == std::to_string(extra));
    REQUIRE(kLogger.getMessage(Logger::kMaxMessages - 1).getMessage() ==
            std::to_string(Logger::kMaxMessages + extra - 1));
  }
}

// 8. stream output
TEST_CASE("Logger stream output", "[logger][stream]") {
  resetLogger();

  SECTION("empty logger") {
    std::ostringstream out;
    out << kLogger;
    REQUIRE(out.str().find("Logger: 0 messages") != std::string::npos);
    REQUIRE(out.str().find("Messages:") != std::string::npos);
  }

  SECTION("includes count type source and body") {
    Logger::logInfo(kSrc, "hello");
    Logger::logError("db", "boom");

    std::ostringstream out;
    out << kLogger;
    const std::string text = out.str();

    REQUIRE(text.find("Logger: 2 messages") != std::string::npos);
    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find("ERROR") != std::string::npos);
    REQUIRE(text.find(kSrc) != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
    REQUIRE(text.find("db") != std::string::npos);
    REQUIRE(text.find("boom") != std::string::npos);
  }

  SECTION("includes message ids") {
    Logger::logHeartbeat(kSrc, "ping");
    const std::string id = kLogger.getMessage(0).getId();

    std::ostringstream out;
    out << kLogger;
    REQUIRE(out.str().find(id) != std::string::npos);
  }
}

// 9. singleton identity
TEST_CASE("Logger is a singleton", "[logger][singleton]") {
  resetLogger();

  Logger &a = Logger::getInstance();
  Logger &b = Logger::getInstance();
  REQUIRE(&a == &b);
  REQUIRE(&a == &kLogger);

  Logger::logInfo(kSrc, "via a");
  REQUIRE(Logger::size() == 1);
  REQUIRE(b.getMessage(0).getMessage() == "via a");
  REQUIRE(kLogger.getMessage(0).getMessage() == "via a");
}

// 10. heartbeat messages get unique ids
TEST_CASE("Logger heartbeat messages receive unique ids", "[logger][heartbeat][id]") {
  resetLogger();

  Logger::logHeartbeat(kSrc, "ping");
  Logger::logHeartbeat(kSrc, "ping");
  Logger::logHeartbeat(kSrc, "pong");

  REQUIRE(Logger::size() == 3);
  const LogMessage a = kLogger.getMessage(0);
  const LogMessage b = kLogger.getMessage(1);
  const LogMessage c = kLogger.getMessage(2);

  REQUIRE_FALSE(a.getId().empty());
  REQUIRE_FALSE(b.getId().empty());
  REQUIRE_FALSE(c.getId().empty());
  REQUIRE(a.getId() != b.getId());
  REQUIRE(a != b);
  REQUIRE(b != c);
  REQUIRE(a.getType() == LogMessage::Type::HEARTBEAT);
  REQUIRE(b.getType() == LogMessage::Type::HEARTBEAT);
  REQUIRE(c.getType() == LogMessage::Type::HEARTBEAT);
}

// 11. mixed types
TEST_CASE("Logger mixed types keep distinct ids and fields", "[logger][mixed]") {
  resetLogger();

  Logger::logInfo("auth", "login");
  Logger::logWarning("auth", "retry");
  Logger::logError("db", "timeout");
  Logger::logHeartbeat("net", "tick");

  REQUIRE(Logger::size() == 4);

  std::unordered_set<std::string> ids;
  for (std::size_t i = 0; i < Logger::size(); ++i) {
    REQUIRE(ids.insert(kLogger.getMessage(i).getId()).second);
  }
  REQUIRE(ids.size() == 4);

  REQUIRE(kLogger.getMessage(0).getType() == LogMessage::Type::INFO);
  REQUIRE(kLogger.getMessage(1).getType() == LogMessage::Type::WARNING);
  REQUIRE(kLogger.getMessage(2).getType() == LogMessage::Type::ERROR);
  REQUIRE(kLogger.getMessage(3).getType() == LogMessage::Type::HEARTBEAT);

  REQUIRE(kLogger.getMessage(0).getSource() == "auth");
  REQUIRE(kLogger.getMessage(2).getSource() == "db");
  REQUIRE(kLogger.getMessage(3).getSource() == "net");
}

// 12. size tracking
TEST_CASE("Logger size tracks additions and clear", "[logger][size]") {
  resetLogger();
  REQUIRE(Logger::size() == 0);

  Logger::logInfo(kSrc, "1");
  REQUIRE(Logger::size() == 1);

  Logger::logWarning(kSrc, "2");
  Logger::logError(kSrc, "3");
  REQUIRE(Logger::size() == 3);

  Logger::clear();
  REQUIRE(Logger::size() == 0);

  for (int i = 0; i < 10; ++i) {
    Logger::logInfo(kSrc, std::to_string(i));
  }
  REQUIRE(Logger::size() == 10);
}
