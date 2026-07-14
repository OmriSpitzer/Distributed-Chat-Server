/**
 * MessageManager header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/message.h"
#include <string>

class MessageManager {
public:
  // send a private message
  bool sendPrivate(const Message &message);

  // save a message
  bool saveMessage(const Message &message);

  // load a message history
  bool loadHistory(const std::string &roomId);
};
