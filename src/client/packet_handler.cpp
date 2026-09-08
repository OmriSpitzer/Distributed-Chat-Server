/**
 * PacketHandler class
 *
 * @brief Dispatches server responses to the appropriate handler.
 * @date 07-09-2026
 */

#include "client/packet_handler.h"
#include "utils/RESPONSE_CODES.h"
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
    // Login and Register packets
  case Packet::PacketType::LOGIN:
  case Packet::PacketType::REGISTER: {
    if (auto user = handleLogin(packet)) {
      result = std::any{*user};
    }
    break;
  }

  // Incoming chat push (not a User response)
  case Packet::PacketType::MESSAGE: {
    std::cout << "[" << packet.sender << "]: " << packet.message << std::endl;
    break;
  }
  default:
    break;
  }

  return result;
}

// handling a login packet
std::optional<User> PacketHandler::handleLogin(const Packet &packet) {
  if (packet.responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
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