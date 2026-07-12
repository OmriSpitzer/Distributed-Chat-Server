/**
 * Logger header file class
 *
 * @date 12-07-2026
 */
#pragma once

#include "utils/models/message.h"
#include <ostream>
#include <string_view>
#include <vector>

class Logger {
public:
  Logger();
  void logInfo(std::string_view message);
  void logWarning(std::string_view message);
  void logError(std::string_view message);
  Message getMessage(int index) const;
  friend std::ostream &operator<<(std::ostream &out, const Logger &logger);

private:
  std::vector<Message> _messages;
  int _message_count;
  void addMessage(std::string_view message, Message::Type type);
};
