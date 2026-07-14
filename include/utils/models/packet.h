/**
 * Packet header file class
 *
 * @date 14-07-2026
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
  };

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
};