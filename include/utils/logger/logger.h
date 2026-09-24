/**
 * Logger class header file (Facade & Singleton Design Pattern)
 *
 * @date 24-09-2026
 */
#pragma once
#include "utils/logger/i_logger.h"
#include "utils/logger/log_message.h"
#include <cstddef>
#include <deque>
#include <mutex>
#include <ostream>
#include <string_view>
#include <vector>

class Logger {
public:
  // maximum number of messages to store
  static constexpr std::size_t MAX_MESSAGES = 1000;

  // singleton instance getter
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  // get a message by index
  LogMessage getMessage(std::size_t index) const;

  // clear stored messages
  static void clear();

  // get the size of the logger
  static std::size_t size();

  // print the logger
  friend std::ostream &operator<<(std::ostream &out, const Logger &logger);

  // delete copy constructor and assignment operator
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

  // add a child sink
  void addLogger(ILogger *logger);

  // clear registered sinks
  void clearLoggers();

  // destructor
  ~Logger() = default;

  // logging methods
  static void logInfo(std::string_view source, std::string_view message);
  static void logWarning(std::string_view source, std::string_view message);
  static void logError(std::string_view source, std::string_view message);
  static void logHeartbeat(std::string_view source, std::string_view message);

private:
  // constructor
  Logger() = default;

  // add a message
  void addMessage(std::string_view source, std::string_view message, LogMessage::Type type);

  mutable std::mutex messages_mutex; // mutex for messages and child sinks
  std::deque<LogMessage> messages;   // messages queue
  std::vector<ILogger *> loggers;    // child sinks (console, file, ...)
};
