/**
 * Network header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/packet.h"

class Network {
public:
  // connecting to the main server
  bool connect();

  // disconnecting from the main server
  void disconnect();

  // sending a packet to the main server
  bool sendPacket(Packet &packet);

  // receiving a packet from the server
  Packet receivePacket();
};