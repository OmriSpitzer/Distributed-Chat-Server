/**
 * Logger class
 *
 * @brief Logger class to store a list of messages and its metadata (message count, messages)
 * @date 04-09-2026
 */

#include "utils/models/logger.h"
#include "utils/models/log_message.h"
#include <iostream>
#include <queue>
#include <string_view>

// singleton constructor design pattern
Logger::Logger() : message_count(0) {}

// log an info message
void Logger::logInfo(std::string_view source, std::string_view message) {
  getInstance().addMessage(source, message, LogMessage::Type::INFO);
}

// log a warning message
void Logger::logWarning(std::string_view source, std::string_view message) {
  getInstance().addMessage(source, message, LogMessage::Type::WARNING);
}

// log an error message
void Logger::logError(std::string_view source, std::string_view message) {
  getInstance().addMessage(source, message, LogMessage::Type::ERROR);
}

// log a heartbeat message
void Logger::logHeartbeat(std::string_view source, std::string_view message) {
  LogMessage logMessage(source, message, LogMessage::Type::HEARTBEAT);

  // print the message
  std::cout << logMessage << "\n";
}

// get a message by index
LogMessage Logger::getMessage(int index) const {
  auto copy = Logger::getInstance().messages;
  for (int i = 0; i < index; i++) {
    copy.pop();
  }
  return copy.front();
}

// print the logger
std::ostream &operator<<(std::ostream &out, const Logger &logger) {
  out << "Logger: " << Logger::getInstance().message_count << " messages\n";
  out << "Messages:\n";

  auto copy = Logger::getInstance().messages;
  while (!copy.empty()) {
    out << copy.front() << "\n";
    copy.pop();
  }
  out << "\n";
  return out;
}

// add a message to the logger
void Logger::addMessage(std::string_view source, std::string_view message, LogMessage::Type type) {
  LogMessage logMessage(source, message, type);

  // add the message to the logger
  getInstance().messages.push(logMessage);
  ++getInstance().message_count;

  // print the message
  std::cout << logMessage << "\n";
}
