/**
 * LogMessage class implementation file
 *
 * @brief LogMessage class to store a message and its metadata
 * @date 12-09-2026
 *
 * LogMessage class with fields: id, source, message, type, timestamp
 * Used for storing and displaying log messages in the server
 */

#include "utils/models/log_message.h"
#include <atomic>
#include <ctime>
#include <ostream>
#include <string>
#include <unordered_map>

// next message id
namespace {
std::atomic<uint64_t> next_message_id{0};
} // namespace

// type to string map
static const std::unordered_map<LogMessage::Type, std::string> logTypeToStringMap = {
    {LogMessage::Type::INFO, "INFO"},
    {LogMessage::Type::WARNING, "WARNING"},
    {LogMessage::Type::ERROR, "ERROR"},
    {LogMessage::Type::HEARTBEAT, "HEARTBEAT"}};

// constructor
LogMessage::LogMessage(std::string_view source, std::string_view message, LogMessage::Type type)
    : id(std::to_string(++next_message_id)), source(source), message(message), type(type),
      timestamp(std::time(nullptr)) {}

// print operator
std::ostream &operator<<(std::ostream &out, const LogMessage &s) {
  out << "[Msg: " << s.id << "] (" << LogMessage::typeToString(s.type) << ", " << s.timestamp
      << ")\n";
  out << s.source << ": " << s.message << '\n';
  return out;
}

// equality operator
bool LogMessage::operator==(const LogMessage &other) const { return id == other.id; }
bool LogMessage::operator!=(const LogMessage &other) const { return !(*this == other); }

// getters
const std::string &LogMessage::getId() const { return id; }
const std::string &LogMessage::getSource() const { return source; }
const std::string &LogMessage::getMessage() const { return message; }
LogMessage::Type LogMessage::getType() const { return type; }
std::time_t LogMessage::getTimestamp() const { return timestamp; }

// type to string
std::string LogMessage::typeToString(LogMessage::Type type) {
  auto it = logTypeToStringMap.find(type);
  if (it != logTypeToStringMap.end()) {
    return it->second;
  }
  return logTypeToStringMap.at(LogMessage::Type::INFO);
}

// string to type
LogMessage::Type LogMessage::stringToType(std::string_view type) {
  for (const auto &[key, value] : logTypeToStringMap) {
    if (value == type)
      return key;
  }
  return LogMessage::Type::INFO;
}