/**
 * PacketHandler header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <any>
#include <optional>

class PacketHandler {
public:
  // dispatch a packet to the matching handler
  std::optional<std::any> handlePacket(const Packet &packet);

private:
  // handling a login packet
  std::optional<User> handleLogin(const Packet &packet);
};
