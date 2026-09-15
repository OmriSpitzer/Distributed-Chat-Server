/**
 * PacketBuilder class
 *
 * @brief Constructs typed packets to send to the server.
 * @date 13-09-2026
 */

#include "client/packet_builder.h"
#include "utils/models/packet.h"
#include <stdexcept>
#include <string_view>

// build a login packet
Packet PacketBuilder::buildLogin(std::string_view username, std::string_view password) {
  if (username.empty() || password.empty()) {
    throw std::invalid_argument("Username and password are required");
  }

  return Packet(username, "", Packet::PacketType::LOGIN, "", password);
}

// build a register packet
Packet PacketBuilder::buildRegister(std::string_view username, std::string_view password,
                                    std::string_view email) {
  if (username.empty() || password.empty() || email.empty()) {
    throw std::invalid_argument("Username, password and email are required");
  }

  return Packet(username, "", Packet::PacketType::REGISTER, email, password);
}

// build a logout packet
Packet PacketBuilder::buildLogout(const User &user) {
  if (user.getUsername().empty() || user.getEmail().empty()) {
    throw std::invalid_argument("Username and email are required");
  }

  return Packet(user.getUsername(), "", Packet::PacketType::LOGOUT, "", user.getEmail());
}

// build a message packet
Packet PacketBuilder::buildMessage(std::string_view username, std::string_view message) {
  if (username.empty() || message.empty()) {
    throw std::invalid_argument("Username and message are required");
  }

  return Packet(username, "", Packet::PacketType::MESSAGE, "", message);
}

// build a join room packet
Packet PacketBuilder::buildJoinRoom(std::string_view username, std::string_view room) {
  if (username.empty() || room.empty()) {
    throw std::invalid_argument("Username and room are required");
  }

  return Packet(username, "", Packet::PacketType::ROOM_JOIN, room, "");
}

// build a leave room packet
Packet PacketBuilder::buildLeaveRoom(std::string_view username, std::string_view room) {
  if (username.empty() || room.empty()) {
    throw std::invalid_argument("Username and room are required");
  }

  return Packet(username, "", Packet::PacketType::ROOM_LEAVE, room, "");
}

// build a update user packet
Packet PacketBuilder::buildUpdateUser(std::string_view username, std::string_view password,
                                      std::string_view email) {
  if (username.empty() || email.empty()) {
    throw std::invalid_argument("Username and email are required");
  }

  return Packet(username, "", Packet::PacketType::UPDATE_USER, email, password);
}

// build a create room packet
Packet PacketBuilder::buildCreateRoom(std::string_view username, std::string_view room,
                                      std::string_view privacy) {
  if (username.empty() || room.empty()) {
    throw std::invalid_argument("Username and room are required");
  }

  return Packet(username, "", Packet::PacketType::ROOM_CREATE, room, privacy);
}

// build an invite packet
Packet PacketBuilder::buildInviteToRoom(std::string_view username, std::string_view room,
                                        std::string_view inviteeUsername) {
  if (username.empty() || room.empty() || inviteeUsername.empty()) {
    throw std::invalid_argument("Username, room and invitee are required");
  }

  return Packet(username, "", Packet::PacketType::ROOM_INVITE, room, inviteeUsername);
}

// build a load message history packet
Packet PacketBuilder::buildLoadMessageHistory(std::string_view username, std::string_view room) {
  if (username.empty() || room.empty()) {
    throw std::invalid_argument("Username and room are required");
  }

  return Packet(username, "", Packet::PacketType::LOAD_MESSAGE_HISTORY, room, "");
}