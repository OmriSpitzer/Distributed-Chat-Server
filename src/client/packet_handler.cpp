/**
 * PacketHandler class
 *
 * @brief Dispatches server responses to the appropriate handler.
 * @date 14-07-2026
 */

#include "client/packet_handler.h"
#include <iostream>

void PacketHandler::handleMessage(const Packet &packet) {
  std::cout << packet.sender << ": " << packet.message << '\n';
}

void PacketHandler::handleLogin(const Packet &packet) {
  std::cout << "Login response: " << packet.message << '\n';
}

void PacketHandler::handleRoomList(const Packet &packet) {
  std::cout << "Rooms: " << packet.message << '\n';
}

void PacketHandler::handleError(const Packet &packet) {
  std::cout << "Error: " << packet.message << '\n';
}
