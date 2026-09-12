/**
 * Message class
 *
 * @brief Message class to store a message and its metadata.
 * @date 11-09-2026
 */

#include "utils/models/message.h"
#include "config/config.h"
#include <atomic>
#include <ctime>
#include <random>

// unique message id generator
namespace {
// local counter
std::atomic<uint64_t> next_message_id{0};

// boot id
const std::string boot_id =
    std::to_string(static_cast<uint64_t>(std::time(nullptr)) * 1000003ull + std::random_device{}());

// make a message id
std::string makeMessageId(std::time_t now) {
  const auto seq = ++next_message_id;
  return config::NODE_ID + "-" + boot_id + "-" + std::to_string(now) + "-" + std::to_string(seq);
}
} // namespace

// constructor
Message::Message(const User &from, const User &to, std::string_view message)
    : from(from), to(to), content(message), timestamp(std::time(nullptr)),
      id(makeMessageId(timestamp)) {}

// reconstruct from stored row
Message::Message(const User &from, const User &to, std::string_view message, std::string id,
                 std::time_t timestamp)
    : from(from), to(to), content(message), timestamp(timestamp), id(std::move(id)) {}

// getters
const User &Message::getFrom() const { return this->from; }
const User &Message::getTo() const { return this->to; }
const std::string &Message::getContent() const { return this->content; }
std::time_t Message::getTimestamp() const { return this->timestamp; }
const std::string &Message::getId() const { return this->id; }

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
bool Message::operator<=(const Message &other) const { return *this < other || *this == other; }
bool Message::operator>=(const Message &other) const { return *this > other || *this == other; }