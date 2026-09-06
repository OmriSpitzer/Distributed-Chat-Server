/**
 * Packet class
 *
 * @date 04-09-2026
 */

#include "utils/models/packet.h"
#include <ctime>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr char kFieldSep = '|';
constexpr char kEscape = '\\';
constexpr std::size_t kFieldCount = 6;

// escape field
std::string escapeField(const std::string &field) {
  std::string out;
  out.reserve(field.size());
  for (char c : field) {
    if (c == kEscape || c == kFieldSep) {
      out.push_back(kEscape);
    }
    out.push_back(c);
  }
  return out;
}

// split fields
std::vector<std::string> splitFields(const std::string &data) {
  std::vector<std::string> parts;
  std::string current;
  bool escaped = false;

  for (char c : data) {
    if (escaped) {
      current.push_back(c);
      escaped = false;
      continue;
    }
    if (c == kEscape) {
      escaped = true;
      continue;
    }
    if (c == kFieldSep) {
      parts.push_back(std::move(current));
      current.clear();
      continue;
    }
    current.push_back(c);
  }

  if (escaped) {
    throw std::invalid_argument("Packet payload ends with a dangling escape");
  }

  parts.push_back(std::move(current));
  return parts;
}

} // namespace

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

// serialize
std::string Packet::serialize() const {
  return packetTypeToString(type) + kFieldSep + escapeField(sender) + kFieldSep +
         escapeField(receiver) + kFieldSep + escapeField(room) + kFieldSep + escapeField(message) +
         kFieldSep + std::to_string(timestamp);
}

// deserialize
Packet Packet::deserialize(const std::string &data) {
  if (data.empty()) {
    throw std::invalid_argument("Packet payload is empty");
  }

  const std::vector<std::string> parts = splitFields(data);
  if (parts.size() != kFieldCount) {
    throw std::invalid_argument("Packet payload must have 6 fields");
  }

  Packet packet;
  packet.type = stringToPacketType(parts[0]);
  packet.sender = parts[1];
  packet.receiver = parts[2];
  packet.room = parts[3];
  packet.message = parts[4];
  try {
    packet.timestamp = std::stoull(parts[5]);
  } catch (const std::exception &) {
    throw std::invalid_argument("Packet timestamp is invalid");
  }
  return packet;
}

// copy the packet
Packet Packet::copy() const {
  Packet packet(sender, receiver, type, room, message);
  packet.timestamp = timestamp;
  return packet;
}
