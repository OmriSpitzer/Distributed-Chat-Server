/**
 * MessageManager header file class
 *
 * @date 04-09-2026
 */
#pragma once
#include "utils/models/message.h"
#include <string>
#include <vector>

class MessageManager {
public:
  // send a message
  bool send(const Message &message);

  // load a message history
  std::vector<Message> loadHistory(const std::string &roomId);
};
