/**
 * Logger Class unit tests
 *
 * @brief Includes: empty state, log types, source/message edges, ordering,
 * getMessage bounds, clear, capacity eviction, stream output, singleton,
 * heartbeat unique ids, mixed types, size tracking, facade sink fan-out,
 * nullptr / throwing / late / reentrant sinks, clear vs sinks, concurrency,
 * ConsoleLogger stderr for ERROR.
 * @date 12-09-2026
 */

#include "utils/logger/consoleLogger.h"
#include "utils/logger/i_logger.h"
#include "utils/logger/logger.h"
#include "utils/logger/log_message.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>


/**
 * 1. empty state after clear
 * 2. logInfo / logWarning / logError / logHeartbeat
 * 3. source and message edge cases
 * 4. messages keep insertion order
 * 5. getMessage bounds
 * 6. clear removes all messages
 * 7. capacity eviction at MAX_MESSAGES
 * 8. stream output
 * 9. singleton identity
 * 10. heartbeat messages get unique ids
 * 11. mixed types
 * 12. size tracking
 * 13. facade fan-out to ILogger sinks
 * 14. ConsoleLogger is an ILogger sink
 * 15. addLogger(nullptr) ignored
 * 16. clear keeps registered sinks
 * 17. late addLogger gets future messages only
 * 18. throwing sink skips later sinks (ring still stores)
 * 19. reentrant log from sink does not deadlock
 * 20. concurrent log / size / getMessage
 * 21. ConsoleLogger ERROR goes to stderr
 */

// minimal globals
static Logger &kLogger = Logger::getInstance();
static const char *kSrc = "logger_test";

static void resetLogger() {
  Logger::clear();
  kLogger.clearLoggers();
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
    REQUIRE_THROWS_AS(kLogger.getMessage(Logger::MAX_MESSAGES), std::out_of_range);
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

// 7. capacity eviction at MAX_MESSAGES
TEST_CASE("Logger evicts oldest messages past MAX_MESSAGES", "[logger][capacity]") {
  resetLogger();

  SECTION("fills exactly to capacity") {
    for (std::size_t i = 0; i < Logger::MAX_MESSAGES; ++i) {
      Logger::logInfo(kSrc, std::to_string(i));
    }
    REQUIRE(Logger::size() == Logger::MAX_MESSAGES);
    REQUIRE(kLogger.getMessage(0).getMessage() == "0");
    REQUIRE(kLogger.getMessage(Logger::MAX_MESSAGES - 1).getMessage() ==
            std::to_string(Logger::MAX_MESSAGES - 1));
  }

  SECTION("one past capacity drops the oldest") {
    for (std::size_t i = 0; i < Logger::MAX_MESSAGES; ++i) {
      Logger::logInfo(kSrc, std::to_string(i));
    }
    Logger::logInfo(kSrc, "overflow");

    REQUIRE(Logger::size() == Logger::MAX_MESSAGES);
    REQUIRE(kLogger.getMessage(0).getMessage() == "1");
    REQUIRE(kLogger.getMessage(Logger::MAX_MESSAGES - 1).getMessage() == "overflow");
  }

  SECTION("many past capacity keeps only the newest window") {
    const std::size_t extra = 50;
    for (std::size_t i = 0; i < Logger::MAX_MESSAGES + extra; ++i) {
      Logger::logWarning(kSrc, std::to_string(i));
    }

    REQUIRE(Logger::size() == Logger::MAX_MESSAGES);
    REQUIRE(kLogger.getMessage(0).getMessage() == std::to_string(extra));
    REQUIRE(kLogger.getMessage(Logger::MAX_MESSAGES - 1).getMessage() ==
            std::to_string(Logger::MAX_MESSAGES + extra - 1));
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

namespace {

// recording sink for facade fan-out tests (process-lifetime; cleared via clearLoggers)
class RecordingLogger : public ILogger {
public:
  mutable std::atomic<int> infoCount{0};
  mutable std::atomic<int> warningCount{0};
  mutable std::atomic<int> errorCount{0};
  mutable std::atomic<int> heartbeatCount{0};
  mutable std::string lastSource;
  mutable std::string lastMessage;

  void log(const LogMessage &message) override {
    switch (message.getType()) {
    case LogMessage::Type::INFO:
      ++infoCount;
      break;
    case LogMessage::Type::WARNING:
      ++warningCount;
      break;
    case LogMessage::Type::ERROR:
      ++errorCount;
      break;
    case LogMessage::Type::HEARTBEAT:
      ++heartbeatCount;
      break;
    }
    lastSource = message.getSource();
    lastMessage = message.getMessage();
  }

  void resetCounts() const {
    infoCount = 0;
    warningCount = 0;
    errorCount = 0;
    heartbeatCount = 0;
    lastSource.clear();
    lastMessage.clear();
  }
};

static RecordingLogger kRecording;
static ConsoleLogger kConsole;

} // namespace

// 13. facade fan-out to ILogger sinks
TEST_CASE("Logger facade forwards to registered sinks", "[logger][facade][sink]") {
  resetLogger();
  kRecording.resetCounts();
  kLogger.addLogger(&kRecording);

  Logger::logInfo(kSrc, "via facade");
  Logger::logWarning(kSrc, "warn");
  Logger::logError(kSrc, "err");
  Logger::logHeartbeat(kSrc, "beat");

  REQUIRE(Logger::size() == 4);
  REQUIRE(kRecording.infoCount == 1);
  REQUIRE(kRecording.warningCount == 1);
  REQUIRE(kRecording.errorCount == 1);
  REQUIRE(kRecording.heartbeatCount == 1);
  REQUIRE(kRecording.lastSource == kSrc);
  REQUIRE(kRecording.lastMessage == "beat");

  SECTION("duplicate addLogger is ignored") {
    kRecording.resetCounts();
    kLogger.addLogger(&kRecording);
    Logger::logInfo(kSrc, "once");
    REQUIRE(kRecording.infoCount == 1);
  }

  SECTION("clearLoggers stops fan-out") {
    kLogger.clearLoggers();
    kRecording.resetCounts();
    Logger::logInfo(kSrc, "no sink");
    REQUIRE(kRecording.infoCount == 0);
    REQUIRE(Logger::size() >= 1);
  }
}

// 14. ConsoleLogger is an ILogger sink
TEST_CASE("ConsoleLogger registers as facade sink", "[logger][facade][console]") {
  resetLogger();
  kLogger.addLogger(&kConsole);
  kLogger.addLogger(&kRecording);
  kRecording.resetCounts();

  Logger::logInfo(kSrc, "console path");
  REQUIRE(Logger::size() == 1);
  REQUIRE(kLogger.getMessage(0).getMessage() == "console path");
  REQUIRE(kRecording.infoCount == 1);
}

namespace {

class ThrowingLogger : public ILogger {
public:
  mutable std::atomic<int> callCount{0};

  void log(const LogMessage &) override {
    ++callCount;
    throw std::runtime_error("sink failed");
  }
};

class ReentrantLogger : public ILogger {
public:
  mutable std::atomic<int> callCount{0};

  void log(const LogMessage &message) override {
    const int n = ++callCount;
    if (n == 1 && message.getMessage() == "outer") {
      Logger::logInfo(kSrc, "inner");
    }
  }
};

static ThrowingLogger kThrowing;
static ReentrantLogger kReentrant;

} // namespace

// 15. addLogger(nullptr) ignored
TEST_CASE("Logger addLogger ignores nullptr", "[logger][facade][nullptr]") {
  resetLogger();
  kRecording.resetCounts();

  kLogger.addLogger(nullptr);
  kLogger.addLogger(&kRecording);
  kLogger.addLogger(nullptr);

  Logger::logInfo(kSrc, "after null");
  REQUIRE(Logger::size() == 1);
  REQUIRE(kRecording.infoCount == 1);
  REQUIRE(kRecording.lastMessage == "after null");
}

// 16. clear keeps registered sinks
TEST_CASE("Logger clear does not unregister sinks", "[logger][clear][sink]") {
  resetLogger();
  kLogger.addLogger(&kRecording);
  kRecording.resetCounts();

  Logger::logInfo(kSrc, "before clear");
  REQUIRE(kRecording.infoCount == 1);

  Logger::clear();
  REQUIRE(Logger::size() == 0);

  kRecording.resetCounts();
  Logger::logInfo(kSrc, "after clear");
  REQUIRE(Logger::size() == 1);
  REQUIRE(kRecording.infoCount == 1);
  REQUIRE(kRecording.lastMessage == "after clear");
}

// 17. late addLogger gets future messages only
TEST_CASE("Logger late addLogger receives future messages only", "[logger][facade][late]") {
  resetLogger();
  kRecording.resetCounts();

  Logger::logInfo(kSrc, "before sink");
  REQUIRE(Logger::size() == 1);
  REQUIRE(kRecording.infoCount == 0);

  kLogger.addLogger(&kRecording);
  Logger::logInfo(kSrc, "after sink");

  REQUIRE(Logger::size() == 2);
  REQUIRE(kLogger.getMessage(0).getMessage() == "before sink");
  REQUIRE(kLogger.getMessage(1).getMessage() == "after sink");
  REQUIRE(kRecording.infoCount == 1);
  REQUIRE(kRecording.lastMessage == "after sink");
}

// 18. throwing sink skips later sinks (ring still stores)
TEST_CASE("Logger throwing sink skips later sinks", "[logger][facade][throw]") {
  resetLogger();
  kThrowing.callCount = 0;
  kRecording.resetCounts();

  kLogger.addLogger(&kThrowing);
  kLogger.addLogger(&kRecording);

  REQUIRE_THROWS_AS(Logger::logInfo(kSrc, "boom"), std::runtime_error);

  REQUIRE(Logger::size() == 1);
  REQUIRE(kLogger.getMessage(0).getMessage() == "boom");
  REQUIRE(kThrowing.callCount == 1);
  REQUIRE(kRecording.infoCount == 0);
}

// 19. reentrant log from sink does not deadlock
TEST_CASE("Logger reentrant sink log does not deadlock", "[logger][facade][reentrant]") {
  resetLogger();
  kReentrant.callCount = 0;
  kLogger.addLogger(&kReentrant);

  Logger::logInfo(kSrc, "outer");

  REQUIRE(Logger::size() == 2);
  REQUIRE(kLogger.getMessage(0).getMessage() == "outer");
  REQUIRE(kLogger.getMessage(1).getMessage() == "inner");
  REQUIRE(kReentrant.callCount == 2);
}

// 20. concurrent log / size / getMessage
TEST_CASE("Logger concurrent log size and getMessage", "[logger][concurrency]") {
  resetLogger();

  constexpr int kThreads = 8;
  constexpr int kPerThread = 100;
  const std::size_t expected =
      static_cast<std::size_t>(kThreads) * static_cast<std::size_t>(kPerThread);

  std::vector<std::thread> writers;
  writers.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    writers.emplace_back([t] {
      for (int i = 0; i < kPerThread; ++i) {
        Logger::logInfo(kSrc, "t" + std::to_string(t) + "-" + std::to_string(i));
      }
    });
  }

  std::atomic<bool> readersStop{false};
  std::thread reader([&] {
    while (!readersStop.load()) {
      const std::size_t n = Logger::size();
      if (n == 0) {
        continue;
      }
      try {
        (void)kLogger.getMessage(n - 1);
      } catch (const std::out_of_range &) {
      }
    }
  });

  for (std::thread &thread : writers) {
    thread.join();
  }
  readersStop = true;
  reader.join();

  REQUIRE(Logger::size() == (std::min)(expected, Logger::MAX_MESSAGES));
  REQUIRE_NOTHROW(kLogger.getMessage(0));
  REQUIRE_NOTHROW(kLogger.getMessage(Logger::size() - 1));
}

// 21. ConsoleLogger ERROR goes to stderr
TEST_CASE("ConsoleLogger ERROR writes to stderr", "[logger][console][stderr]") {
  ConsoleLogger console;
  const LogMessage errorMsg(kSrc, "err-line", LogMessage::Type::ERROR);
  const LogMessage infoMsg(kSrc, "info-line", LogMessage::Type::INFO);

  std::ostringstream errCapture;
  std::ostringstream outCapture;
  std::streambuf *oldErr = std::cerr.rdbuf(errCapture.rdbuf());
  std::streambuf *oldOut = std::cout.rdbuf(outCapture.rdbuf());

  console.log(errorMsg);
  console.log(infoMsg);

  std::cerr.rdbuf(oldErr);
  std::cout.rdbuf(oldOut);

  REQUIRE(errCapture.str().find("err-line") != std::string::npos);
  REQUIRE(errCapture.str().find("info-line") == std::string::npos);
  REQUIRE(outCapture.str().find("info-line") != std::string::npos);
  REQUIRE(outCapture.str().find("err-line") == std::string::npos);
}

