/**
 * Packet class implementation file
 *
 * @brief Packet class to store a packet and its metadata
 * @date 12-09-2026
 *
 * Packet class with fields: type, sender, receiver, room, message, timestamp, responseCode
 * Used for sending and receiving packets between the server and the clients
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
    {Packet::PacketType::UPDATE_USER, "UPDATE_USER"},
    {Packet::PacketType::ROOM_CREATE, "ROOM_CREATE"},
    {Packet::PacketType::ROOM_LIST, "ROOM_LIST"},
    {Packet::PacketType::LOAD_MESSAGE_HISTORY, "LOAD_MESSAGE_HISTORY"},
    {Packet::PacketType::ROOM_INVITE, "ROOM_INVITE"},
    {Packet::PacketType::ROOM_DELETE, "ROOM_DELETE"},
    {Packet::PacketType::ROOM_KICK, "ROOM_KICK"},
    {Packet::PacketType::SERVER_DIRECTORY, "SERVER_DIRECTORY"},
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
  for (const auto &[key, value] : packet_type_to_string) {
    if (value == type)
      return key;
  }
  return PacketType::DEFAULT;
}

// check if a packet type is valid
bool Packet::isValidPacketType(PacketType type) {
  return packet_type_to_string.find(type) != packet_type_to_string.end();
}