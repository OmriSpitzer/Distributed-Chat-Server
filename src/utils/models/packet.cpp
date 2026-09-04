/**
 * Packet class
 *
 * @date 03-09-2026
 */

#include "utils/models/packet.h"
#include <ctime>

// constructor
Packet::Packet(std::string sender, std::string receiver, PacketType type = PacketType::NONE,
               std::string room = "", std::string message = "") {
  this->type = type;
  this->sender = sender;
  this->receiver = receiver;
  this->room = room;
  this->message = message;
  this->timestamp = static_cast<uint64_t>(std::time(nullptr));
}

// copy the packet
Packet Packet::copy() const {
  Packet packet(sender, receiver, type, room, message);
  return packet;
}