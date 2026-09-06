/**
 * Packet class
 *
 * @date 04-09-2026
 */

#include "utils/models/packet.h"
#include <ctime>
#include <stdexcept>
#include <string>

// constructor
Packet::Packet() : type(PacketType::DEFAULT), timestamp(0) {}
Packet::Packet(std::string sender, std::string receiver, PacketType type, std::string room,
               std::string message) {
  this->type = type;
  this->sender = sender;
  this->receiver = receiver;
  this->room = room;
  this->message = message;
  this->timestamp = static_cast<uint64_t>(std::time(nullptr));
}

// convert packet type to string
std::string Packet::packetTypeToString(PacketType type) {
  switch (type) {
  case PacketType::LOGIN:
    return "LOGIN";
  case PacketType::LOGOUT:
    return "LOGOUT";
  case PacketType::MESSAGE:
    return "MESSAGE";
  case PacketType::ROOM_JOIN:
    return "ROOM_JOIN";
  case PacketType::ROOM_LEAVE:
    return "ROOM_LEAVE";
  case PacketType::DEFAULT:
    return "DEFAULT";
  case PacketType::HEARTBEAT:
    return "HEARTBEAT";
  case PacketType::REGISTER:
    return "REGISTER";
  }
  throw std::invalid_argument("Unknown packet type");
}

// convert string to packet type
Packet::PacketType Packet::stringToPacketType(const std::string &type) {
  if (type == "LOGIN") {
    return PacketType::LOGIN;
  }
  if (type == "LOGOUT") {
    return PacketType::LOGOUT;
  }
  if (type == "MESSAGE") {
    return PacketType::MESSAGE;
  }
  if (type == "ROOM_JOIN") {
    return PacketType::ROOM_JOIN;
  }
  if (type == "ROOM_LEAVE") {
    return PacketType::ROOM_LEAVE;
  }
  if (type == "DEFAULT") {
    return PacketType::DEFAULT;
  }
  if (type == "HEARTBEAT") {
    return PacketType::HEARTBEAT;
  }
  if (type == "REGISTER") {
    return PacketType::REGISTER;
  }
  throw std::invalid_argument("Unknown packet type: " + type);
}

// copy the packet
Packet Packet::copy() const {
  Packet packet(sender, receiver, type, room, message);
  packet.timestamp = timestamp;
  return packet;
}
