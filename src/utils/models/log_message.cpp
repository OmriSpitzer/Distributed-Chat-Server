/**
 * LogMessage class
 *
 * @brief LogMessage class to store a message and its metadata (id, message, type, timestamp)
 * @date 04-09-2026
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
LogMessage::LogMessage(std::string_view source, std::string_view message, LogMessage::Type type) {
  if (type != LogMessage::Type::HEARTBEAT) {
    id = std::to_string(++next_message_id);
  }

  this->source = source;
  this->message = message;
  this->type = type;
  timestamp = std::time(nullptr);
}

// print
std::ostream &operator<<(std::ostream &out, const LogMessage &s) {
  out << "[Msg: " << s.id << "] (" << LogMessage::typeToString(s.type) << ", " << s.timestamp
      << ")\n";
  out << s.source << ": " << s.message << '\n';
  return out;
}

// equals
bool LogMessage::operator==(const LogMessage &other) const { return id == other.id; }

std::string LogMessage::typeToString(LogMessage::Type type) {
  switch (type) {
  case LogMessage::Type::INFO:
    return "INFO";
  case LogMessage::Type::WARNING:
    return "WARNING";
  case LogMessage::Type::ERROR:
    return "ERROR";
  case LogMessage::Type::HEARTBEAT:
    return "HEARTBEAT";
  }
  return "UNKNOWN";
}
