#pragma once

#include <ctime>
#include <ostream>
#include <string>
#include <string_view>

class LogMessage {
public:
  // message type enum class
  enum class Type { INFO, WARNING, ERROR };

  // constructor
  LogMessage(std::string_view message, LogMessage::Type type);

  // getters
  std::string getId() const;
  std::string getMessage() const;
  std::time_t getTimestamp() const;
  LogMessage::Type getType() const;

  // setters
  void setMessage(std::string_view message);
  void setType(LogMessage::Type type);
  void setTimestamp(std::time_t timestamp);

  // print operator
  friend std::ostream &operator<<(std::ostream &out, const LogMessage &msg);

  // equality operator
  bool operator==(const LogMessage &other) const;

private:
  std::string _id;                                        // message id
  std::string _message;                                   // message content
  LogMessage::Type _type;                                 // message type
  std::time_t _timestamp;                                 // message timestamp
  static std::string typeToString(LogMessage::Type type); // type to string
};
