/**
 * PacketBuilder class
 *
 * @brief Constructs typed packets to send to the server.
 * @date 06-09-2026
 */

#include "client/packet_builder.h"
#include "utils/models/packet.h"
#include <ctime>

// build a login packet
Packet PacketBuilder::buildLogin(const std::string &username, const std::string &password) {
  Packet packet;
  packet.type = Packet::PacketType::LOGIN;
  packet.sender = username;
  packet.message = password;
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}

// build a register packet
Packet PacketBuilder::buildRegister(const std::string &username, const std::string &password,
                                    const std::string &email) {
  Packet packet;
  packet.type = Packet::PacketType::REGISTER;
  packet.sender = username;
  packet.message = password + "|" + email;
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}

// build a logout packet
Packet PacketBuilder::buildLogout(const User &user) {
  Packet packet;
  packet.type = Packet::PacketType::LOGOUT;
  packet.sender = user.getUsername();
  packet.message = user.getEmail();
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}

// build a message packet
Packet PacketBuilder::buildMessage(const std::string &username, const std::string &message) {
  Packet packet;
  packet.type = Packet::PacketType::MESSAGE;
  packet.sender = username;
  packet.message = message;
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}

// build a join room packet
Packet PacketBuilder::buildJoinRoom(const std::string &username, const std::string &room) {
  Packet packet;
  packet.type = Packet::PacketType::ROOM_JOIN;
  packet.sender = username;
  packet.room = room;
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}

// build a leave room packet
Packet PacketBuilder::buildLeaveRoom(const std::string &username, const std::string &room) {
  Packet packet;
  packet.type = Packet::PacketType::ROOM_LEAVE;
  packet.sender = username;
  packet.room = room;
  packet.timestamp = static_cast<uint64_t>(std::time(nullptr));
  return packet;
}