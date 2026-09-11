/**
 * MessageManager class
 *
 * @brief Builds a gossip event and fans it out via ConnectionManager::rumor.
 * @date 10-09-2026
 */

#include "server/message_manager.h"
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"

// send a message — local apply + peer fan-out happen inside rumor()
bool MessageManager::send(const Message &message, const Room &room, ConnectionManager &connections,
                          int skipSocket) {
  (void)skipSocket; // applyEvent broadcasts to all local room sockets

  const std::string payload =
      "MESSAGE|" + message.getId() + "|" + message.getFrom().getUsername() + "|" +
      message.getContent() + "|" +
      std::to_string(static_cast<long long>(message.getTimestamp()));

  Packet event(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, room.getName(), payload);
  connections.rumor(event);

  Logger::logInfo("MessageManager", "Message rumor: " + message.getContent());
  return true;
}

// load a message history
std::vector<Message> MessageManager::loadHistory(const std::string &roomId) {
  const std::vector<Message> messages = DatabaseManager::getInstance().loadHistory(roomId);

  Logger::logInfo("MessageManager", "Loading message history for room: " + roomId);
  return messages;
}
