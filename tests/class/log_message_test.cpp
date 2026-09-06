/**
 * LogMessage Class unit tests
 */

#include "utils/models/log_message.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

static const char *kSource = "logger";

TEST_CASE("LogMessage stream output includes source, message, and type", "[log_message][ctor]") {
  SECTION("INFO") {
    const LogMessage msg(kSource, "hello", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
  }

  SECTION("WARNING") {
    const LogMessage msg(kSource, "watch out", LogMessage::Type::WARNING);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("WARNING") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("watch out") != std::string::npos);
  }

  SECTION("ERROR") {
    const LogMessage msg(kSource, "failed", LogMessage::Type::ERROR);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("ERROR") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("failed") != std::string::npos);
  }

  SECTION("empty message") {
    const LogMessage msg(kSource, "", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
  }

  SECTION("empty source") {
    const LogMessage msg("", "hello", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
  }

  SECTION("multiline message") {
    const LogMessage msg(kSource, "line1\nline2", LogMessage::Type::INFO);
    std::ostringstream out;
    out << msg;
    const std::string text = out.str();

    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("line1\nline2") != std::string::npos);
  }
}

TEST_CASE("Successive log messages are not equal", "[log_message][id]") {
  const LogMessage first(kSource, "one", LogMessage::Type::INFO);
  const LogMessage second(kSource, "one", LogMessage::Type::INFO);

  REQUIRE_FALSE(first == second);
}

TEST_CASE("A log message compares equal only to itself", "[log_message][equality]") {
  const LogMessage msg(kSource, "same", LogMessage::Type::INFO);

  REQUIRE(msg == msg);
}
