/**
 * Logger class implementation file (Facade & Singleton Design Pattern)
 *
 * @brief Facade singleton: stores messages in a ring and forwards to ILogger sinks.
 *
 * Design patterns: Facade, Singleton
 * Used for storing and displaying log messages; optional sinks via addLogger.
 * @date 24-09-2026
 */
#include "utils/logger/logger.h"
#include "utils/logger/log_message.h"
#include <stdexcept>
#include <string_view>
#include <vector>

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

// register a child sink
void Logger::addLogger(ILogger *logger) {
  if (logger == nullptr) {
    return;
  }

  std::lock_guard lock(messages_mutex);
  for (const ILogger *existing : loggers) {
    if (existing == logger) {
      return;
    }
  }
  loggers.push_back(logger);
}

// clear registered sinks
void Logger::clearLoggers() {
  std::lock_guard lock(messages_mutex);
  loggers.clear();
}

// add a message
void Logger::addMessage(std::string_view source, std::string_view message, LogMessage::Type type) {
  LogMessage logMessage(source, message, type);

  // create the message and forward it to the sinks
  std::vector<ILogger *> sinks;
  {
    std::lock_guard lock(messages_mutex);
    messages.push_back(logMessage);
    while (messages.size() > MAX_MESSAGES) {
      messages.pop_front();
    }
    sinks = loggers;
  }

  // forward the message to the sinks
  for (ILogger *sink : sinks) {
    sink->log(logMessage);
  }
}
