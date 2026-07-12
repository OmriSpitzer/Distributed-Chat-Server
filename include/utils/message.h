#pragma once

#include <ctime>
#include <ostream>
#include <string>
#include <string_view>

class Message {
public:
  enum class Type { INFO, WARNING, ERROR };

  Message(std::string_view message, Message::Type type);
  std::string getId() const;
  std::string getMessage() const;
  std::time_t getTimestamp() const;
  Message::Type getType() const;
  void setMessage(std::string_view message);
  void setType(Message::Type type);
  void setTimestamp(std::time_t timestamp);
  friend std::ostream &operator<<(std::ostream &out, const Message &msg);
  bool operator==(const Message &other) const;

private:
  std::string _id;
  std::string _message;
  Message::Type _type;
  std::time_t _timestamp;
  static std::string typeToString(Message::Type type);
};
