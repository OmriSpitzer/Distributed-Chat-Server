/**
 * PacketProcessor header file class
 *
 * @date 06-09-2026
 */
#pragma once
#include "auth/authentication.h"
#include "server/client_session.h"
#include "server/message_manager.h"
#include "server/user_manager.h"
#include "utils/models/packet.h"

class ConnectionManager; // forward declaration

class PacketProcessor {
public:
  // process a packet
  static Packet processPacket(const Packet &packet, ClientSession &session,
                              ConnectionManager &connections);

  // process a heartbeat packet
  static Packet processHeartbeatPacket(const Packet &packet, ConnectionManager &connections);

private:
  // authentication
  Authentication authentication;

  // message manager
  MessageManager messageManager;

  // user manager
  UserManager userManager;
};