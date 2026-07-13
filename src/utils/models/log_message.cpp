/**
 * LogMessage class
 *
 * @brief LogMessage class to store a message and its metadata (id, message, type, timestamp)
 * @date 12-07-2026
 */

#include "utils/models/log_message.h"
#include <atomic>
#include <ctime>
#include <ostream>
#include <string>

namespace {
std::atomic<uint64_t> next_message_id{0};
}

// constructor
LogMessage::LogMessage(std::string_view message, Type type) {
  _id = std::to_string(++next_message_id);
  _message = message;
  _type = type;
  _timestamp = std::time(nullptr);
}

// getters
std::string LogMessage::getId() const { return _id; }
std::string LogMessage::getMessage() const { return _message; }
std::time_t LogMessage::getTimestamp() const { return _timestamp; }
LogMessage::Type LogMessage::getType() const { return _type; }

// setters
void LogMessage::setMessage(std::string_view message) { _message = message; }
void LogMessage::setType(LogMessage::Type type) { _type = type; }
void LogMessage::setTimestamp(std::time_t timestamp) { _timestamp = timestamp; }

// print
std::ostream &operator<<(std::ostream &out, const LogMessage &s) {
  out << "[Msg: " << s.getId() << "] (" << LogMessage::typeToString(s.getType()) << ", "
      << s.getTimestamp() << ")\n";
  out << "Message: " << s.getMessage() << '\n';
  return out;
}

// equals
bool LogMessage::operator==(const LogMessage &other) const { return _id == other._id; }

std::string LogMessage::typeToString(LogMessage::Type type) {
  switch (type) {
  case LogMessage::Type::INFO:
    return "INFO";
  case LogMessage::Type::WARNING:
    return "WARNING";
  case LogMessage::Type::ERROR:
    return "ERROR";
  }
  return "UNKNOWN";
}
