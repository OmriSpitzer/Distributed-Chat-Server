/**
 * Logger header file class
 *
 * @date 12-07-2026
 */
#pragma once

#include "utils/models/log_message.h"
#include <ostream>
#include <queue>
#include <string_view>

class Logger {
public:
  // singleton instance getter
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  // logging methods
  static void logInfo(std::string_view source, std::string_view message);
  static void logWarning(std::string_view source, std::string_view message);
  static void logError(std::string_view source, std::string_view message);

  // get a message by index
  LogMessage getMessage(int index) const;

  // print the logger
  friend std::ostream &operator<<(std::ostream &out, const Logger &logger);

  // delete copy constructor and assignment operator
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

private:
  std::queue<LogMessage> messages; // messages
  int message_count;               // message count

  // add a message to the logger
  void addMessage(std::string_view source, std::string_view message, LogMessage::Type type);

  // constructor
  Logger();
};
