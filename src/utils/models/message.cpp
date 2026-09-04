/**
 * Message class
 *
 * @brief Message class to store a message and its metadata (from, to, content, timestamp, id)
 * @date 04-09-2026
 */

#include "utils/models/message.h"
#include <atomic>

// unique message id generator
namespace {
std::atomic<uint64_t> next_message_id{0};
}

// constructor
Message::Message(const User &from, const User &to, std::string_view message)
    : from(from), to(to), content(message), timestamp(std::time(nullptr)),
      id(std::to_string(++next_message_id)) {}

// getters
const User &Message::getFrom() const { return this->from; }
const User &Message::getTo() const { return this->to; }
std::string Message::getContent() const { return this->content; }
std::time_t Message::getTimestamp() const { return this->timestamp; }
std::string Message::getId() const { return this->id; }

// print operator
std::ostream &operator<<(std::ostream &out, const Message &msg) {
  out << "Message from " << msg.from.getUsername() << " to " << msg.to.getUsername()
      << " (t:" << msg.timestamp << ", id: " << msg.id << ")\n";
  out << "Content: " << msg.content << '\n';
  return out;
}

// equality operator
bool Message::operator==(const Message &other) const {
  return this->id == other.id && this->timestamp == other.timestamp;
}
bool Message::operator!=(const Message &other) const { return !(*this == other); }

// comparison operators
bool Message::operator<(const Message &other) const { return this->timestamp < other.timestamp; }
bool Message::operator>(const Message &other) const { return this->timestamp > other.timestamp; }
bool Message::operator<=(const Message &other) const {
  return this->timestamp <= other.timestamp || *this == other;
}
bool Message::operator>=(const Message &other) const {
  return this->timestamp >= other.timestamp || *this == other;
}