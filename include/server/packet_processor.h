/**
 * PacketProcessor header file class
 *
 * @date 14-07-2026
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
  Packet processPacket(const Packet &packet, ClientSession &session);

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