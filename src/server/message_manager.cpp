/**
 * MessageManager class
 *
 * @brief Sends and persists chat messages via the database.
 * @date 10-09-2026
 */

#include "server/message_manager.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"

class ConnectionManager;

// send a message
bool MessageManager::send(const Message &message, const Room &room, ConnectionManager &connections,
                          int skipSocket) {
  DatabaseManager &db = DatabaseManager::getInstance();
  if (!db.saveMessage(message, room.getName())) {
    Logger::logError("MessageManager", "Failed to save message: " + message.getContent());
    return false;
  }

  const Packet push(message.getFrom().getUsername(), "", Packet::PacketType::MESSAGE,
                    room.getName(), message.getContent(), 0);
  RoomManager::getInstance().broadcast(room, push, connections, skipSocket);
  Logger::logInfo("MessageManager", "Message sent: " + message.getContent());
  return true;
}

// load a message history
std::vector<Message> MessageManager::loadHistory(const std::string &roomId) {
  const std::vector<Message> messages = DatabaseManager::getInstance().loadHistory(roomId);

  Logger::logInfo("MessageManager", "Loading message history for room: " + roomId);
  return messages;
}
