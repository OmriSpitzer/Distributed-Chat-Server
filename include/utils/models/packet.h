/**
 * Packet header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include <cstdint>
#include <string>

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
  };

  // constructors
  Packet();
  Packet(std::string sender, std::string receiver, PacketType type = PacketType::DEFAULT,
         std::string room = "", std::string message = "", int responseCode = 0);

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

  // copy the packet
  Packet copy() const;

  // convert packet type to string
  static std::string packetTypeToString(PacketType type);

  // convert string to packet type
  static PacketType stringToPacketType(const std::string &type);
};
