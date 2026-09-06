/**
 * LogMessage header file class
 *
 * @date 04-09-2026
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

private:
  std::string id;        // message id
  std::string source;    // message source
  std::string message;   // message content
  LogMessage::Type type; // message type
  std::time_t timestamp; // message timestamp

  // type to string
  static std::string typeToString(LogMessage::Type type);
};
