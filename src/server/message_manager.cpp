/**
 * MessageManager class
 *
 * @brief Sends and persists chat messages via the database.
 * @date 04-09-2026
 */

#include "server/message_manager.h"
#include "server/database_manager.h"
#include "utils/models/logger.h"
#include <iostream>

// send a message
bool MessageManager::send(const Message &message) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: send the message to a user and save it in the database
  Logger::logInfo("MessageManager", "Message sent: " + message.getContent());
  return true;
}

// load a message history
std::vector<Message> MessageManager::loadHistory(const std::string &roomId) {
  std::vector<Message> messages;
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: load the message history from the database

  Logger::logInfo("MessageManager", "Loading message history for room: " + roomId);
  return messages;
}
