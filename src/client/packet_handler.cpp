/**
 * PacketHandler class
 *
 * @brief Dispatches server responses to the appropriate handler.
 * @date 13-09-2026
 */

#include "client/packet_handler.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <exception>
#include <optional>

// dispatch a packet to the matching handler
std::optional<User> PacketHandler::handlePacket(const Packet &packet) {
  switch (packet.type) {
  case Packet::PacketType::LOGIN:
  case Packet::PacketType::REGISTER:
  case Packet::PacketType::UPDATE_USER:
    return handleLogin(packet);
  default:
    return std::nullopt;
  }
}

// handling a login / register success payload
std::optional<User> PacketHandler::handleLogin(const Packet &packet) {
  if (packet.responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    return std::nullopt;
  }

  try {
    return User::deserialize(packet.message);
  } catch (const std::exception &) {
    return std::nullopt;
  }
}
