/**
 * MessageManager header file class
 *
 * @date 10-09-2026
 */
#pragma once
#include "connection_manager.h"
#include "utils/models/message.h"
#include "utils/models/room.h"
#include <string>
#include <vector>


class MessageManager {
public:
  // build GOSSIP_EVENT payload and fan-out via ConnectionManager::rumor
  bool send(const Message &message, const Room &room, ConnectionManager &connections,
            int skipSocket = -1);

  // load a message history
  std::vector<Message> loadHistory(const std::string &roomId);
};
