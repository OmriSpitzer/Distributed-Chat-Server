#pragma once

#include <ctime>
#include <ostream>
#include <string>
#include <string_view>

class Message {
public:
  // message type enum class
  enum class Type { INFO, WARNING, ERROR };

  // constructor
  Message(std::string_view message, Message::Type type);

  // getters
  std::string getId() const;
  std::string getMessage() const;
  std::time_t getTimestamp() const;
  Message::Type getType() const;

  // setters
  void setMessage(std::string_view message);
  void setType(Message::Type type);
  void setTimestamp(std::time_t timestamp);

  // print operator
  friend std::ostream &operator<<(std::ostream &out, const Message &msg);

  // equality operator
  bool operator==(const Message &other) const;

private:
  std::string _id;                                     // message id
  std::string _message;                                // message content
  Message::Type _type;                                 // message type
  std::time_t _timestamp;                              // message timestamp
  static std::string typeToString(Message::Type type); // type to string
};
