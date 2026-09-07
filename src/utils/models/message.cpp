/**
 * Message class
 *
 * @brief Message class to store a message and its metadata (from, to, content, timestamp, id)
 * @date 07-09-2026
 */

#include "utils/models/message.h"
#include "config/config.h"
#include <atomic>
#include <ctime>

// unique message id generator
namespace {
// message id : <nodeId>-<unix_time>-<local_counter>
// local counter
std::atomic<uint64_t> next_message_id{0};

// id prefix
std::string idPrefix = config::NODE_ID + "-" + std::to_string(std::time(nullptr)) + "-";
} // namespace

// constructor
Message::Message(const User &from, const User &to, std::string_view message)
    : from(from), to(to), content(message), timestamp(std::time(nullptr)),
      id(idPrefix + std::to_string(++next_message_id)) {}

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