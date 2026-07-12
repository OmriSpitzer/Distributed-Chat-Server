/**
 * Logger class
 *
 * @brief Logger class to store a list of messages and its metadata (message count, messages)
 * @date 12-07-2026
 */

#include "utils/models/logger.h"

// constructor
Logger::Logger() : _message_count(0) {}

// log an info message
void Logger::logInfo(std::string_view message) { addMessage(message, Message::Type::INFO); }

// log a warning message
void Logger::logWarning(std::string_view message) { addMessage(message, Message::Type::WARNING); }

// log an error message
void Logger::logError(std::string_view message) { addMessage(message, Message::Type::ERROR); }

// get a message by index
Message Logger::getMessage(int index) const {
  return _messages.at(static_cast<std::size_t>(index));
}

// print the logger
std::ostream &operator<<(std::ostream &out, const Logger &logger) {
  out << "Logger: " << logger._message_count << " messages\n";
  out << "Messages:\n";
  for (int i = 0; i < logger._message_count; ++i) {
    out << logger._messages.at(static_cast<std::size_t>(i)) << "\n";
  }
  out << "\n";
  return out;
}

// add a message to the logger
void Logger::addMessage(std::string_view message, Message::Type type) {
  _messages.emplace_back(std::string(message), type);
  ++_message_count;
}
