/**
 * LogMessage Class unit tests
 *
 * @brief Includes: constructor fields, source/message edges, all types, timestamp,
 * unique ids (incl. HEARTBEAT), equality, typeToString, stringToType, round-trip,
 * stream output, getter stability.
 * @date 12-09-2026
 */

#include "utils/models/log_message.h"
#include <catch2/catch_test_macros.hpp>
#include <ctime>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * 1. constructor stores source, message, type
 * 2. source edge cases
 * 3. message content edge cases
 * 4. timestamp is set near construction time
 * 5. successive messages receive unique ids
 * 6. heartbeats also receive unique ids
 * 7. equality and inequality
 * 8. typeToString
 * 9. stringToType
 * 10. type conversion round-trip
 * 11. stream output
 * 12. getters
 */

// 1. constructor stores source, message, type
TEST_CASE("LogMessage constructor stores source, message, and type", "[log_message][ctor]") {
  SECTION("INFO") {
    const LogMessage msg("logger", "hello", LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == "logger");
    REQUIRE(msg.getMessage() == "hello");
    REQUIRE(msg.getType() == LogMessage::Type::INFO);
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("WARNING") {
    const LogMessage msg("auth", "watch out", LogMessage::Type::WARNING);
    REQUIRE(msg.getSource() == "auth");
    REQUIRE(msg.getMessage() == "watch out");
    REQUIRE(msg.getType() == LogMessage::Type::WARNING);
  }

  SECTION("ERROR") {
    const LogMessage msg("db", "failed", LogMessage::Type::ERROR);
    REQUIRE(msg.getSource() == "db");
    REQUIRE(msg.getMessage() == "failed");
    REQUIRE(msg.getType() == LogMessage::Type::ERROR);
  }

  SECTION("HEARTBEAT") {
    const LogMessage msg("heartbeat", "ping", LogMessage::Type::HEARTBEAT);
    REQUIRE(msg.getSource() == "heartbeat");
    REQUIRE(msg.getMessage() == "ping");
    REQUIRE(msg.getType() == LogMessage::Type::HEARTBEAT);
    REQUIRE_FALSE(msg.getId().empty());
  }
}

// 2. source edge cases
TEST_CASE("LogMessage source edge cases", "[log_message][ctor][edge]") {
  SECTION("empty source") {
    const LogMessage msg("", "hello", LogMessage::Type::INFO);
    REQUIRE(msg.getSource().empty());
    REQUIRE(msg.getMessage() == "hello");
  }

  SECTION("whitespace only source") {
    const LogMessage msg("   \t  ", "x", LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == "   \t  ");
  }

  SECTION("leading and trailing spaces") {
    const LogMessage msg("  logger  ", "x", LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == "  logger  ");
  }

  SECTION("unicode source") {
    const LogMessage msg("עֹמְרִי", "x", LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == "עֹמְרִי");
  }

  SECTION("special characters in source") {
    const std::string special = "a|b{c}\"d'\\e<>&;%@#";
    const LogMessage msg(special, "x", LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == special);
  }

  SECTION("very long source") {
    const std::string longSource(4096, 's');
    const LogMessage msg(longSource, "x", LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == longSource);
    REQUIRE(msg.getSource().size() == 4096);
  }

  SECTION("single character source") {
    const LogMessage msg("x", "y", LogMessage::Type::INFO);
    REQUIRE(msg.getSource() == "x");
  }
}

// 3. message content edge cases
TEST_CASE("LogMessage message content edge cases", "[log_message][ctor][edge]") {
  SECTION("empty message") {
    const LogMessage msg("logger", "", LogMessage::Type::INFO);
    REQUIRE(msg.getMessage().empty());
    REQUIRE_FALSE(msg.getId().empty());
  }

  SECTION("whitespace only") {
    const LogMessage msg("logger", "   \t  ", LogMessage::Type::INFO);
    REQUIRE(msg.getMessage() == "   \t  ");
  }

  SECTION("leading and trailing spaces") {
    const LogMessage msg("logger", "  hello  ", LogMessage::Type::INFO);
    REQUIRE(msg.getMessage() == "  hello  ");
  }

  SECTION("multiline message") {
    const LogMessage msg("logger", "line1\nline2\r\nline3", LogMessage::Type::INFO);
    REQUIRE(msg.getMessage() == "line1\nline2\r\nline3");
  }

  SECTION("unicode message") {
    const LogMessage msg("logger", "שלום \xCE\xB1\xCE\xB2\xCE\xB3", LogMessage::Type::INFO);
    REQUIRE(msg.getMessage() == "שלום \xCE\xB1\xCE\xB2\xCE\xB3");
  }

  SECTION("special characters") {
    const std::string special = "a|b{c}\"d'\\e<>&;%@#";
    const LogMessage msg("logger", special, LogMessage::Type::ERROR);
    REQUIRE(msg.getMessage() == special);
  }

  SECTION("embedded null byte") {
    const std::string withNull("hello\0world", 11);
    const LogMessage msg("logger", withNull, LogMessage::Type::WARNING);
    REQUIRE(msg.getMessage() == withNull);
    REQUIRE(msg.getMessage().size() == 11);
  }

  SECTION("long message") {
    const std::string longContent(100000, 'x');
    const LogMessage msg("logger", longContent, LogMessage::Type::INFO);
    REQUIRE(msg.getMessage() == longContent);
    REQUIRE(msg.getMessage().size() == 100000);
  }

  SECTION("single character") {
    const LogMessage msg("logger", "x", LogMessage::Type::INFO);
    REQUIRE(msg.getMessage() == "x");
  }

  SECTION("both source and message empty") {
    const LogMessage msg("", "", LogMessage::Type::HEARTBEAT);
    REQUIRE(msg.getSource().empty());
    REQUIRE(msg.getMessage().empty());
    REQUIRE(msg.getType() == LogMessage::Type::HEARTBEAT);
    REQUIRE_FALSE(msg.getId().empty());
  }
}

// 4. timestamp is set near construction time
TEST_CASE("LogMessage timestamp is set near construction time", "[log_message][timestamp]") {
  const auto before = std::time(nullptr);
  const LogMessage msg("logger", "ping", LogMessage::Type::INFO);
  const auto after = std::time(nullptr);

  REQUIRE(msg.getTimestamp() >= before);
  REQUIRE(msg.getTimestamp() <= after);
}

// 5. successive messages receive unique ids
TEST_CASE("Successive log messages receive unique ids", "[log_message][id]") {
  SECTION("two messages") {
    const LogMessage first("logger", "one", LogMessage::Type::INFO);
    const LogMessage second("logger", "two", LogMessage::Type::INFO);

    REQUIRE(first.getId() != second.getId());
    REQUIRE(first != second);
    REQUIRE_FALSE(first == second);
  }

  SECTION("many messages") {
    std::unordered_set<std::string> ids;
    for (int i = 0; i < 200; ++i) {
      const LogMessage msg("logger", "n" + std::to_string(i), LogMessage::Type::INFO);
      REQUIRE(ids.insert(msg.getId()).second);
    }
    REQUIRE(ids.size() == 200);
  }

  SECTION("same content still unique ids") {
    const LogMessage a("logger", "same", LogMessage::Type::INFO);
    const LogMessage b("logger", "same", LogMessage::Type::INFO);
    REQUIRE(a.getMessage() == b.getMessage());
    REQUIRE(a.getSource() == b.getSource());
    REQUIRE(a.getType() == b.getType());
    REQUIRE(a.getId() != b.getId());
  }

  SECTION("mixed types still unique ids") {
    std::unordered_set<std::string> ids;
    const std::vector<LogMessage::Type> types = {
        LogMessage::Type::INFO, LogMessage::Type::WARNING, LogMessage::Type::ERROR,
        LogMessage::Type::HEARTBEAT};
    for (const auto type : types) {
      const LogMessage msg("src", "body", type);
      REQUIRE(ids.insert(msg.getId()).second);
    }
    REQUIRE(ids.size() == types.size());
  }
}

// 6. heartbeats also receive unique ids
TEST_CASE("Heartbeat log messages receive unique ids", "[log_message][id][heartbeat]") {
  SECTION("two heartbeats are not equal") {
    const LogMessage a("heartbeat", "ping", LogMessage::Type::HEARTBEAT);
    const LogMessage b("heartbeat", "ping", LogMessage::Type::HEARTBEAT);

    REQUIRE_FALSE(a.getId().empty());
    REQUIRE_FALSE(b.getId().empty());
    REQUIRE(a.getId() != b.getId());
    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
  }

  SECTION("many heartbeats") {
    std::unordered_set<std::string> ids;
    for (int i = 0; i < 100; ++i) {
      const LogMessage msg("heartbeat", "tick", LogMessage::Type::HEARTBEAT);
      REQUIRE(ids.insert(msg.getId()).second);
    }
    REQUIRE(ids.size() == 100);
  }

  SECTION("heartbeat vs info with same body") {
    const LogMessage hb("src", "same", LogMessage::Type::HEARTBEAT);
    const LogMessage info("src", "same", LogMessage::Type::INFO);
    REQUIRE(hb.getId() != info.getId());
    REQUIRE(hb != info);
  }
}

// 7. equality and inequality
TEST_CASE("LogMessage equality compares by id only", "[log_message][equality]") {
  SECTION("message equals itself") {
    const LogMessage msg("logger", "same", LogMessage::Type::INFO);
    REQUIRE(msg == msg);
    REQUIRE_FALSE(msg != msg);
  }

  SECTION("different generated messages are not equal") {
    const LogMessage a("logger", "x", LogMessage::Type::INFO);
    const LogMessage b("logger", "x", LogMessage::Type::INFO);
    REQUIRE(a != b);
    REQUIRE_FALSE(a == b);
  }

  SECTION("copy shares id and compares equal") {
    const LogMessage original("logger", "body", LogMessage::Type::WARNING);
    const LogMessage copy = original;
    REQUIRE(copy.getId() == original.getId());
    REQUIRE(copy == original);
    REQUIRE_FALSE(copy != original);
  }

  SECTION("copied then another new message is not equal") {
    const LogMessage original("logger", "body", LogMessage::Type::ERROR);
    const LogMessage copy = original;
    const LogMessage other("logger", "body", LogMessage::Type::ERROR);
    REQUIRE(copy == original);
    REQUIRE(other != original);
    REQUIRE(other != copy);
  }

  SECTION("equality ignores source message and type differences when id matches via copy") {
    const LogMessage original("a", "b", LogMessage::Type::INFO);
    LogMessage twin = original;
    REQUIRE(twin == original);
    REQUIRE(twin.getSource() == original.getSource());
    REQUIRE(twin.getMessage() == original.getMessage());
    REQUIRE(twin.getType() == original.getType());
  }
}

// 8. typeToString
TEST_CASE("LogMessage typeToString", "[log_message][typeToString]") {
  REQUIRE(LogMessage::typeToString(LogMessage::Type::INFO) == "INFO");
  REQUIRE(LogMessage::typeToString(LogMessage::Type::WARNING) == "WARNING");
  REQUIRE(LogMessage::typeToString(LogMessage::Type::ERROR) == "ERROR");
  REQUIRE(LogMessage::typeToString(LogMessage::Type::HEARTBEAT) == "HEARTBEAT");

  SECTION("invalid enum value defaults to INFO") {
    const auto bogus = static_cast<LogMessage::Type>(999);
    REQUIRE(LogMessage::typeToString(bogus) == "INFO");
  }
}

// 9. stringToType
TEST_CASE("LogMessage stringToType", "[log_message][stringToType]") {
  REQUIRE(LogMessage::stringToType("INFO") == LogMessage::Type::INFO);
  REQUIRE(LogMessage::stringToType("WARNING") == LogMessage::Type::WARNING);
  REQUIRE(LogMessage::stringToType("ERROR") == LogMessage::Type::ERROR);
  REQUIRE(LogMessage::stringToType("HEARTBEAT") == LogMessage::Type::HEARTBEAT);

  SECTION("unknown / edge strings default to INFO") {
    REQUIRE(LogMessage::stringToType("") == LogMessage::Type::INFO);
    REQUIRE(LogMessage::stringToType("info") == LogMessage::Type::INFO);
    REQUIRE(LogMessage::stringToType("Info") == LogMessage::Type::INFO);
    REQUIRE(LogMessage::stringToType("ERROR ") == LogMessage::Type::INFO);
    REQUIRE(LogMessage::stringToType(" ERROR") == LogMessage::Type::INFO);
    REQUIRE(LogMessage::stringToType("UNKNOWN") == LogMessage::Type::INFO);
    REQUIRE(LogMessage::stringToType("heartbeat") == LogMessage::Type::INFO);
    REQUIRE(LogMessage::stringToType("HEART BEAT") == LogMessage::Type::INFO);
  }
}

// 10. type conversion round-trip
TEST_CASE("LogMessage type conversion round-trip", "[log_message][type-roundtrip]") {
  const std::vector<LogMessage::Type> types = {
      LogMessage::Type::INFO, LogMessage::Type::WARNING, LogMessage::Type::ERROR,
      LogMessage::Type::HEARTBEAT};

  for (const auto t : types) {
    REQUIRE(LogMessage::stringToType(LogMessage::typeToString(t)) == t);
  }
}

// 11. stream output
TEST_CASE("LogMessage stream output", "[log_message][stream]") {
  SECTION("INFO includes type source message and id") {
    const LogMessage msg("logger", "hello", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find("logger") != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
    REQUIRE(text.find(msg.getId()) != std::string::npos);
    REQUIRE(text.find(std::to_string(msg.getTimestamp())) != std::string::npos);
  }

  SECTION("WARNING") {
    const LogMessage msg("auth", "watch out", LogMessage::Type::WARNING);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("WARNING") != std::string::npos);
    REQUIRE(text.find("auth") != std::string::npos);
    REQUIRE(text.find("watch out") != std::string::npos);
  }

  SECTION("ERROR") {
    const LogMessage msg("db", "failed", LogMessage::Type::ERROR);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("ERROR") != std::string::npos);
    REQUIRE(text.find("db") != std::string::npos);
    REQUIRE(text.find("failed") != std::string::npos);
  }

  SECTION("HEARTBEAT") {
    const LogMessage msg("heartbeat", "ping", LogMessage::Type::HEARTBEAT);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("HEARTBEAT") != std::string::npos);
    REQUIRE(text.find("heartbeat") != std::string::npos);
    REQUIRE(text.find("ping") != std::string::npos);
    REQUIRE(text.find(msg.getId()) != std::string::npos);
  }

  SECTION("empty message still prints type and source") {
    const LogMessage msg("logger", "", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find("logger") != std::string::npos);
  }

  SECTION("empty source still prints type and message") {
    const LogMessage msg("", "hello", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
  }

  SECTION("multiline message preserved") {
    const LogMessage msg("logger", "line1\nline2", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("line1\nline2") != std::string::npos);
  }

  SECTION("Msg prefix present") {
    const LogMessage msg("s", "m", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    REQUIRE(out.str().find("[Msg: ") != std::string::npos);
  }
}

// 12. getters
TEST_CASE("LogMessage getters return stored fields", "[log_message][getters]") {
  SECTION("all getters match constructor args") {
    const LogMessage msg("src", "body", LogMessage::Type::ERROR);
    REQUIRE(msg.getSource() == "src");
    REQUIRE(msg.getMessage() == "body");
    REQUIRE(msg.getType() == LogMessage::Type::ERROR);
    REQUIRE_FALSE(msg.getId().empty());
    REQUIRE(msg.getTimestamp() > 0);
  }

  SECTION("string getters return stable references") {
    const LogMessage msg("src", "body", LogMessage::Type::INFO);
    const std::string &id1 = msg.getId();
    const std::string &id2 = msg.getId();
    const std::string &src1 = msg.getSource();
    const std::string &src2 = msg.getSource();
    const std::string &body1 = msg.getMessage();
    const std::string &body2 = msg.getMessage();

    REQUIRE(&id1 == &id2);
    REQUIRE(&src1 == &src2);
    REQUIRE(&body1 == &body2);
  }

  SECTION("id is numeric string") {
    const LogMessage msg("src", "body", LogMessage::Type::INFO);
    for (const char c : msg.getId()) {
      REQUIRE(c >= '0');
      REQUIRE(c <= '9');
    }
  }
}
