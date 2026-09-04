/**
 * Packet header file class
 *
 * @date 04-09-2026
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
  };

  // constructors
  Packet() : type(PacketType::DEFAULT), timestamp(0) {}
  Packet(std::string sender, std::string receiver, PacketType type = PacketType::DEFAULT,
         std::string room = "", std::string message = "");

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

  // copy the packet
  Packet copy() const;
};