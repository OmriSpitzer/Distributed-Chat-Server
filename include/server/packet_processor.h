/**
 * PacketProcessor header file class
 *
 * @date 06-09-2026
 */
#pragma once
#include "server/client_session.h"
#include "server/user_manager.h"
#include "utils/models/packet.h"

class ConnectionManager; // forward declaration

class PacketProcessor {
public:
  // process a packet
  static Packet processPacket(const Packet &packet, ClientSession &session,
                              ConnectionManager &connections);

private:
  // user manager
  UserManager userManager;
};