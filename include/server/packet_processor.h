/**
 * PacketProcessor header file class
 *
 * @date 04-09-2026
 */
#pragma once
#include "auth/authentication.h"
#include "server/client_session.h"
#include "server/message_manager.h"
#include "server/room_manager.h"
#include "server/user_manager.h"
#include "utils/models/packet.h"

class PacketProcessor {
public:
  // process a packet
  static Packet processPacket(const Packet &packet, ClientSession &session);

  // process a heartbeat packet
  static Packet processHeartbeatPacket(const Packet &packet);

private:
  // authentication
  Authentication authentication;

  // room manager
  RoomManager roomManager;

  // message manager
  MessageManager messageManager;

  // user manager
  UserManager userManager;
};