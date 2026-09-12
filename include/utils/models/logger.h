/**
 * Logger header file class
 *
 * @date 12-09-2026
 */

#pragma once
#include "utils/models/log_message.h"
#include <cstddef>
#include <deque>
#include <mutex>
#include <ostream>
#include <string_view>

class Logger {
public:
  static constexpr std::size_t kMaxMessages = 1000;

  // singleton instance getter
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  // logging methods
  static void logInfo(std::string_view source, std::string_view message);
  static void logWarning(std::string_view source, std::string_view message);
  static void logError(std::string_view source, std::string_view message);
  static void logHeartbeat(std::string_view source, std::string_view message);

  // get a message by index
  LogMessage getMessage(std::size_t index) const;

  // clear stored messages
  static void clear();

  // get the size of the logger
  static std::size_t size();

  // print the logger
  friend std::ostream &operator<<(std::ostream &out, const Logger &logger);

  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;
  Logger(Logger &&) = delete;
  Logger &operator=(Logger &&) = delete;

private:
  mutable std::mutex messages_mutex;
  std::deque<LogMessage> messages;

  void addMessage(std::string_view source, std::string_view message, LogMessage::Type type);

  Logger() = default;
};
