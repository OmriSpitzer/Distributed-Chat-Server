/**
 * Logger class
 *
 * @brief Logger class to store a list of messages.
 * @date 12-09-2026
 */

#include "utils/models/logger.h"
#include "utils/models/log_message.h"
#include <iostream>
#include <stdexcept>
#include <string_view>

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
  getInstance().addMessage(source, message, LogMessage::Type::HEARTBEAT);
}

// get a message by index
LogMessage Logger::getMessage(std::size_t index) const {
  std::lock_guard lock(messages_mutex);
  if (index >= messages.size()) {
    throw std::out_of_range("Logger::getMessage index out of range");
  }
  return messages[index];
}

// clear the logger
void Logger::clear() {
  Logger &logger = getInstance();
  std::lock_guard lock(logger.messages_mutex);
  logger.messages.clear();
}

// get the size of the logger
std::size_t Logger::size() {
  Logger &logger = getInstance();
  std::lock_guard lock(logger.messages_mutex);
  return logger.messages.size();
}

// print the logger
std::ostream &operator<<(std::ostream &out, const Logger &logger) {
  std::lock_guard lock(logger.messages_mutex);
  out << "Logger: " << logger.messages.size() << " messages\n";
  out << "Messages:\n";
  for (const auto &msg : logger.messages) {
    out << msg << "\n";
  }
  out << "\n";
  return out;
}

// add a message to the logger
void Logger::addMessage(std::string_view source, std::string_view message, LogMessage::Type type) {
  LogMessage logMessage(source, message, type);

  {
    std::lock_guard lock(messages_mutex);
    messages.push_back(logMessage);
    while (messages.size() > kMaxMessages) {
      messages.pop_front();
    }
  }

  if (type == LogMessage::Type::ERROR) {
    std::cerr << logMessage << "\n";
  } else {
    std::cout << logMessage << "\n";
  }
}
