/**
 * MessageManager class
 *
 * @brief Sends and persists chat messages via the database.
 * @date 14-07-2026
 */

#include "server/message_manager.h"
#include "server/database_manager.h"
#include <iostream>

bool MessageManager::sendPrivate(const Message &message) {
  std::cout << "Private message from " << message.getFrom().getUsername() << " to "
            << message.getTo().getUsername() << '\n';
  return DatabaseManager::getInstance().saveMessage(message);
}

bool MessageManager::saveMessage(const Message &message) {
  return DatabaseManager::getInstance().saveMessage(message);
}

bool MessageManager::loadHistory(const std::string &roomId) {
  auto history = DatabaseManager::getInstance().loadMessages(roomId);
  std::cout << "Loaded " << history.size() << " messages for room " << roomId << '\n';
  return true;
}
