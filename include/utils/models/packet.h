/**
 * Packet header file class
 *
 * @date 12-09-2026
 */

#pragma once
#include <cstdint>
#include <string>
#include <string_view>

class Packet {
public:
  // packet type enum
  enum class PacketType {
    LOGIN,
    LOGOUT,
    MESSAGE,
    ROOM_JOIN,
    ROOM_LEAVE,
    DEFAULT,
    HEARTBEAT,
    REGISTER,
    GOSSIP_HELLO,
    GOSSIP_EVENT,
    GOSSIP_DIGEST,
    GOSSIP_PULL
  };

  // constructors
  Packet();
  Packet(std::string_view sender, std::string_view receiver, PacketType type = PacketType::DEFAULT,
         std::string_view room = "", std::string_view message = "", int responseCode = 0);

  // packet type
  PacketType type;

  // sender of the packet
  std::string sender;

  // receiver of the packet
  std::string receiver;

  // room of the packet
  std::string room;

  // message of the packet
  std::string message;

  // timestamp of the packet
  uint64_t timestamp;

  // response code
  int responseCode;

  // convert packet type to string
  static std::string packetTypeToString(PacketType type);

  // convert string to packet type
  static PacketType stringToPacketType(std::string_view type);
};
