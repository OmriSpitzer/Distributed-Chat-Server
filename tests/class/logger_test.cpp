/**
 * Logger Class unit tests
 */

#include "utils/models/logger.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

static const char *kSource = "logger";

static std::string dumpLogger() {
  std::ostringstream out;
  out << Logger::getInstance();
  return out.str();
}

static int loggerMessageCount() {
  const std::string text = dumpLogger();
  const std::string prefix = "Logger: ";
  const auto pos = text.find(prefix);
  REQUIRE(pos != std::string::npos);
  return std::stoi(text.substr(pos + prefix.size()));
}

TEST_CASE("Logger records info, warning, and error messages", "[logger][log]") {
  SECTION("INFO") {
    Logger::logInfo(kSource, "hello");
    const std::string text = dumpLogger();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
  }

  SECTION("WARNING") {
    Logger::logWarning(kSource, "watch out");
    const std::string text = dumpLogger();

    REQUIRE(text.find("WARNING") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("watch out") != std::string::npos);
  }

  SECTION("ERROR") {
    Logger::logError(kSource, "failed");
    const std::string text = dumpLogger();

    REQUIRE(text.find("ERROR") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("failed") != std::string::npos);
  }

  SECTION("empty message") {
    Logger::logInfo(kSource, "");
    const std::string text = dumpLogger();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find(kSource) != std::string::npos);
  }

  SECTION("empty source") {
    Logger::logInfo("", "hello");
    const std::string text = dumpLogger();

    REQUIRE(text.find("INFO") != std::string::npos);
    REQUIRE(text.find("hello") != std::string::npos);
  }

  SECTION("multiline message") {
    Logger::logInfo(kSource, "line1\nline2");
    const std::string text = dumpLogger();

    REQUIRE(text.find(kSource) != std::string::npos);
    REQUIRE(text.find("line1\nline2") != std::string::npos);
  }
}

TEST_CASE("getMessage returns the most recently logged entry", "[logger][get]") {
  Logger::logInfo(kSource, "retrievable");
  const int count = loggerMessageCount();
  REQUIRE(count >= 1);

  std::ostringstream out;
  out << Logger::getInstance().getMessage(count - 1);
  const std::string text = out.str();

  REQUIRE(text.find("retrievable") != std::string::npos);
  REQUIRE(text.find("INFO") != std::string::npos);
  REQUIRE(text.find(kSource) != std::string::npos);
}

TEST_CASE("Successive log entries are distinct", "[logger][id]") {
  Logger::logInfo(kSource, "one");
  Logger::logInfo(kSource, "two");
  const int count = loggerMessageCount();
  REQUIRE(count >= 2);

  const LogMessage first = Logger::getInstance().getMessage(count - 2);
  const LogMessage second = Logger::getInstance().getMessage(count - 1);

  REQUIRE_FALSE(first == second);
}

TEST_CASE("getMessage returns the same entry for the same index", "[logger][equality]") {
  Logger::logInfo(kSource, "same");
  const int count = loggerMessageCount();
  REQUIRE(count >= 1);

  const LogMessage a = Logger::getInstance().getMessage(count - 1);
  const LogMessage b = Logger::getInstance().getMessage(count - 1);

  REQUIRE(a == b);
}

TEST_CASE("Logger stream output includes the message count", "[logger][print]") {
  Logger::logInfo(kSource, "counted");
  const std::string text = dumpLogger();
  const int count = loggerMessageCount();

  REQUIRE(text.find("Logger: " + std::to_string(count) + " messages") != std::string::npos);
  REQUIRE(text.find("Messages:") != std::string::npos);
  REQUIRE(text.find("counted") != std::string::npos);
}
