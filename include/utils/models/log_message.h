/**
 * LogMessage header file class
 *
 * @date 12-09-2026
 */

#pragma once
#include <ctime>
#include <ostream>
#include <string>
#include <string_view>

class LogMessage {
public:
  // message type enum class
  enum class Type { INFO, WARNING, ERROR, HEARTBEAT };

  // constructor
  LogMessage(std::string_view source, std::string_view message, LogMessage::Type type);

  // print operator
  friend std::ostream &operator<<(std::ostream &out, const LogMessage &msg);

  // equality operator
  bool operator==(const LogMessage &other) const;
  bool operator!=(const LogMessage &other) const;

  // type to string
  static std::string typeToString(LogMessage::Type type);

  // string to type
  static LogMessage::Type stringToType(std::string_view type);

  // getters
  const std::string &getId() const;
  const std::string &getSource() const;
  const std::string &getMessage() const;
  LogMessage::Type getType() const;
  std::time_t getTimestamp() const;

private:
  std::string id;        // message id
  std::string source;    // message source
  std::string message;   // message content
  LogMessage::Type type; // message type
  std::time_t timestamp; // message timestamp
};
