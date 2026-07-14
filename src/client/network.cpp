/**
 * Network class
 *
 * @brief Client-side network transport (stubbed for this pass).
 * @date 14-07-2026
 */

#include "client/network.h"
#include <ctime>
#include <iostream>

bool Network::connect() {
  std::cout << "Connecting to server...\n";
  return true;
}

void Network::disconnect() { std::cout << "Disconnected from server\n"; }

bool Network::sendPacket(Packet &packet) {
  std::cout << "Sending packet from " << packet.sender << '\n';
  return true;
}

Packet Network::receivePacket() {
  Packet packet;
  packet.type = Packet::PacketType::MESSAGE;
  packet.sender = "server";
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}
