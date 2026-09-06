/**
 * Network header file class
 *
 * @date 04-09-2026
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

  // check if the network is connected
  bool isConnected() const;

private:
  bool connected = false; // whether the network is connected
};