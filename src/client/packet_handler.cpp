/**
 * PacketHandler class
 *
 * @brief Dispatches server responses to the appropriate handler.
 * @date 07-09-2026
 */

#include "client/packet_handler.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <any>
#include <iostream>
#include <optional>
#include <stdexcept>

// dispatch a packet to the matching handler
std::optional<std::any> PacketHandler::handlePacket(const Packet &packet) {
  std::optional<std::any> result = std::nullopt;
  switch (packet.type) {
  case Packet::PacketType::MESSAGE:
    handleMessage(packet);
    break;
  case Packet::PacketType::LOGIN:
    if (auto user = handleLogin(packet)) {
      result = std::any{*user};
    }
    break;
  case Packet::PacketType::REGISTER:
    handleRegister(packet);
    break;
  case Packet::PacketType::ROOM_JOIN:
    handleRoomJoin(packet);
    break;
  case Packet::PacketType::ROOM_LEAVE:
    handleRoomLeave(packet);
    break;
  default:
    break;
  }

  return result;
}

// handling a message packet
void PacketHandler::handleMessage(const Packet &packet) {
  std::cout << packet.sender << ": " << packet.message << '\n';
}

// handling a login packet
std::optional<User> PacketHandler::handleLogin(const Packet &packet) {
  if (packet.responseCode != 200) {
    std::cout << "Login failed: " << packet.message << '\n';
    return std::nullopt;
  }

  try {
    return User::deserialize(packet.message);
  } catch (const std::exception &e) {
    std::cout << "Login failed: " << e.what() << '\n';
    return std::nullopt;
  }
}

// handling a register packet
void PacketHandler::handleRegister(const Packet &packet) {
  std::cout << "Register response: " << packet.message << '\n';
}

// handling a room leave packet
void PacketHandler::handleRoomLeave(const Packet &packet) {
  std::cout << "Left room: " << packet.message << '\n';
}

// handling a room join packet
void PacketHandler::handleRoomJoin(const Packet &packet) {
  std::cout << "Joined room: " << packet.message << '\n';
}
