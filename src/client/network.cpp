/**
 * Network class
 *
 * @brief Client-side network transport (stubbed for this pass).
 * @date 04-09-2026
 */

#include "client/network.h"
#include <ctime>
#include <iostream>

// connecting to the server
bool Network::connect() {
  std::cout << "Connecting to server...\n";
  connected = true;
  return true;
}

// disconnecting from the server
void Network::disconnect() {
  std::cout << "Disconnected from server\n";
  connected = false;
}

// sending a packet to the server
bool Network::sendPacket(Packet &packet) {
  std::cout << "Sending packet from " << packet.sender << '\n';
  if (!connected) {
    std::cout << "Not connected to the server\n";
    return false;
  }
  return true;
}

// receiving a packet from the server
Packet Network::receivePacket() {
  Packet packet;
  packet.type = Packet::PacketType::MESSAGE;
  packet.sender = "server";
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  if (!connected) {
    std::cout << "Not connected to the server\n";
    return Packet();
  }
  return packet;
}

// check if the network is connected
bool Network::isConnected() const { return connected; }