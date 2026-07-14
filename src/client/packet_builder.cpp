/**
 * PacketBuilder class
 *
 * @brief Constructs typed packets to send to the server.
 * @date 14-07-2026
 */

#include "client/packet_builder.h"
#include <ctime>

Packet PacketBuilder::buildLogin(const std::string &username, const std::string &password) {
  Packet packet;
  packet.type = Packet::PacketType::LOGIN;
  packet.sender = username;
  packet.message = password;
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}

Packet PacketBuilder::buildMessage(const Message &msg) {
  Packet packet;
  packet.type = Packet::PacketType::MESSAGE;
  packet.sender = msg.getFrom().getUsername();
  packet.receiver = msg.getTo().getUsername();
  packet.message = msg.getContent();
  packet.timestamp = static_cast<uint64_t>(msg.getTimestamp());
  return packet;
}

Packet PacketBuilder::buildJoinRoom(const std::string &room) {
  Packet packet;
  packet.type = Packet::PacketType::ROOM_JOIN;
  packet.room = room;
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}
