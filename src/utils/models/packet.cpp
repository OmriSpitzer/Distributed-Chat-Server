/**
 * Packet class
 *
 * @brief Packet class to store a packet and its metadata.
 * @date 12-09-2026
 */

#include "utils/models/packet.h"
#include <ctime>
#include <string_view>
#include <unordered_map>

// packet type to string map
static const std::unordered_map<Packet::PacketType, std::string> packet_type_to_string = {
    {Packet::PacketType::LOGIN, "LOGIN"},
    {Packet::PacketType::LOGOUT, "LOGOUT"},
    {Packet::PacketType::MESSAGE, "MESSAGE"},
    {Packet::PacketType::ROOM_JOIN, "ROOM_JOIN"},
    {Packet::PacketType::ROOM_LEAVE, "ROOM_LEAVE"},
    {Packet::PacketType::DEFAULT, "DEFAULT"},
    {Packet::PacketType::HEARTBEAT, "HEARTBEAT"},
    {Packet::PacketType::REGISTER, "REGISTER"},
    {Packet::PacketType::GOSSIP_HELLO, "GOSSIP_HELLO"},
    {Packet::PacketType::GOSSIP_EVENT, "GOSSIP_EVENT"},
    {Packet::PacketType::GOSSIP_DIGEST, "GOSSIP_DIGEST"},
    {Packet::PacketType::GOSSIP_PULL, "GOSSIP_PULL"},
};

// string to packet type map
static const std::unordered_map<std::string, Packet::PacketType> string_to_packet_type = {
    {"LOGIN", Packet::PacketType::LOGIN},
    {"LOGOUT", Packet::PacketType::LOGOUT},
    {"MESSAGE", Packet::PacketType::MESSAGE},
    {"ROOM_JOIN", Packet::PacketType::ROOM_JOIN},
    {"ROOM_LEAVE", Packet::PacketType::ROOM_LEAVE},
    {"DEFAULT", Packet::PacketType::DEFAULT},
    {"HEARTBEAT", Packet::PacketType::HEARTBEAT},
    {"REGISTER", Packet::PacketType::REGISTER},
    {"GOSSIP_HELLO", Packet::PacketType::GOSSIP_HELLO},
    {"GOSSIP_EVENT", Packet::PacketType::GOSSIP_EVENT},
    {"GOSSIP_DIGEST", Packet::PacketType::GOSSIP_DIGEST},
    {"GOSSIP_PULL", Packet::PacketType::GOSSIP_PULL},
};

// constructor
Packet::Packet() : type(Packet::PacketType::DEFAULT), timestamp(0), responseCode(0) {}
Packet::Packet(std::string_view sender, std::string_view receiver, Packet::PacketType type,
               std::string_view room, std::string_view message, int responseCode)
    : type(type), sender(sender), receiver(receiver), room(room), message(message),
      timestamp(static_cast<uint64_t>(std::time(nullptr))), responseCode(responseCode) {}

// type to string
std::string Packet::packetTypeToString(Packet::PacketType type) {
  auto it = packet_type_to_string.find(type);
  if (it != packet_type_to_string.end()) {
    return it->second;
  }
  return packet_type_to_string.at(Packet::PacketType::DEFAULT);
}

// string to type
Packet::PacketType Packet::stringToPacketType(std::string_view type) {
  auto it = string_to_packet_type.find(std::string(type));
  if (it != string_to_packet_type.end()) {
    return it->second;
  }
  return Packet::PacketType::DEFAULT;
}