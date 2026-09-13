/**
 * PacketBuilder header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <string_view>

class PacketBuilder {
public:
  // build a login packet
  static Packet buildLogin(std::string_view username, std::string_view password);

  // build a logout packet
  static Packet buildLogout(const User &user);

  // build a message packet
  static Packet buildMessage(std::string_view username, std::string_view message);

  // build a join room packet
  static Packet buildJoinRoom(std::string_view username, std::string_view room);

  // build a leave room packet
  static Packet buildLeaveRoom(std::string_view username, std::string_view room);

  // build a register packet
  static Packet buildRegister(std::string_view username, std::string_view password,
                              std::string_view email);
};