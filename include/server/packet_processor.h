/**
 * PacketProcessor header file class
 *
 * @date 13-09-2026
 */
#pragma once
#include "server/client_session.h"
#include "utils/models/packet.h"

class ConnectionManager;

class PacketProcessor {
public:
  // process a packet
  static Packet processPacket(const Packet &packet, ClientSession &session,
                              ConnectionManager &connections);
};