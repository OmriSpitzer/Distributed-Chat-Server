/**
 * LogMessage class
 *
 * @brief LogMessage class to store a message and its metadata.
 * @date 12-09-2026
 */

#include "utils/models/log_message.h"
#include <atomic>
#include <ctime>
#include <ostream>
#include <string>
#include <unordered_map>

namespace {
std::atomic<uint64_t> next_message_id{0};
}

// type to string map
static const std::unordered_map<LogMessage::Type, std::string> type_to_string = {
    {LogMessage::Type::INFO, "INFO"},
    {LogMessage::Type::WARNING, "WARNING"},
    {LogMessage::Type::ERROR, "ERROR"},
    {LogMessage::Type::HEARTBEAT, "HEARTBEAT"}};

// string to type map
static const std::unordered_map<std::string, LogMessage::Type> string_to_type = {
    {"INFO", LogMessage::Type::INFO},
    {"WARNING", LogMessage::Type::WARNING},
    {"ERROR", LogMessage::Type::ERROR},
    {"HEARTBEAT", LogMessage::Type::HEARTBEAT}};

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
  auto it = type_to_string.find(type);
  if (it != type_to_string.end()) {
    return it->second;
  }
  return type_to_string.at(LogMessage::Type::INFO);
}

// string to type
LogMessage::Type LogMessage::stringToType(std::string_view type) {
  auto it = string_to_type.find(std::string(type));
  if (it != string_to_type.end()) {
    return it->second;
  }
  return LogMessage::Type::INFO;
}