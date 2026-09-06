/**
 * PacketHandler header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/packet.h"

class PacketHandler {
public:
  // dispatch a packet to the matching handler
  void handlePacket(const Packet &packet);

  // handling a packet
  void handleMessage(const Packet &packet);

  // handling a login packet
  void handleLogin(const Packet &packet);

  // handling a register packet
  void handleRegister(const Packet &packet);

  // handling a room list packet
  void handleRoomList(const Packet &packet);

  // handling an error packet
  void handleError(const Packet &packet);
};
