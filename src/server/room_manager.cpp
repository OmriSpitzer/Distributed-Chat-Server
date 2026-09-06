/**
 * RoomManager class
 *
 * @brief Basic room lifecycle operations backed by the database.
 * @date 06-09-2026
 */

#include "server/room_manager.h"
#include "server/database_manager.h"
#include "utils/models/logger.h"
#include <iostream>

// create a room
bool RoomManager::createRoom(const Room &room) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: create the room in the database
  Logger::logInfo("RoomManager", "Room created: " + room.getName());
  return true;
}

// delete a room
bool RoomManager::deleteRoom(const Room &room) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: delete the room from the database
  Logger::logInfo("RoomManager", "Room deleted: " + room.getName());
  return true;
}

// join a room
bool RoomManager::joinRoom(const Room &room, const User &user) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: join the room in the database
  Logger::logInfo("RoomManager",
                  "User joined room: " + user.getUsername() + " joined room " + room.getName());
  return true;
}

// leave a room
bool RoomManager::leaveRoom(const Room &room, const User &user) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: leave the room in the database
  Logger::logInfo("RoomManager",
                  "User left room: " + user.getUsername() + " left room " + room.getName());
  return true;
}

// broadcast a message to a room
bool RoomManager::broadcast(const Room &room, const std::string &message) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: broadcast the message to the room in the database
  Logger::logInfo("RoomManager",
                  "Message broadcasted to room: " + room.getName() + " message: " + message);
  return true;
}

// broadcast a message to all rooms
bool RoomManager::broadcastAll(const std::string &message) {
  DatabaseManager &db = DatabaseManager::getInstance();

  // #TODO: broadcast the message to all rooms in the database
  Logger::logInfo("RoomManager", "Message broadcasted to all rooms: " + message);
  return true;
}

// Lobby room
const Room RoomManager::LOBBY = Room("Lobby", Room::RoomType::LOBBY);