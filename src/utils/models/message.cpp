/**
 * Message class
 *
 * @brief Message class to store a message and its metadata (id, message, type, timestamp)
 * @date 12-07-2026
 */

#include "utils/models/message.h"
#include <atomic>
#include <ctime>
#include <ostream>
#include <string>

namespace {
std::atomic<uint64_t> next_message_id{0};
}

// constructor
Message::Message(std::string_view message, Type type) {
  _id = std::to_string(++next_message_id);
  _message = message;
  _type = type;
  _timestamp = std::time(nullptr);
}

// getters
std::string Message::getId() const { return _id; }
std::string Message::getMessage() const { return _message; }
std::time_t Message::getTimestamp() const { return _timestamp; }
Message::Type Message::getType() const { return _type; }

// setters
void Message::setMessage(std::string_view message) { _message = message; }
void Message::setType(Message::Type type) { _type = type; }
void Message::setTimestamp(std::time_t timestamp) { _timestamp = timestamp; }

// print
std::ostream &operator<<(std::ostream &out, const Message &s) {
  out << "[Msg: " << s.getId() << "] (" << Message::typeToString(s.getType()) << ", "
      << s.getTimestamp() << ")\n";
  out << "Message: " << s.getMessage() << '\n';
  return out;
}

// equals
bool Message::operator==(const Message &other) const { return _id == other._id; }

std::string Message::typeToString(Message::Type type) {
  switch (type) {
  case Message::Type::INFO:
    return "INFO";
  case Message::Type::WARNING:
    return "WARNING";
  case Message::Type::ERROR:
    return "ERROR";
  }
  return "UNKNOWN";
}
