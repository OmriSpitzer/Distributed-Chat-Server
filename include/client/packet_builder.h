/**
 * PacketBuilder header file class
 *
 * @date 06-09-2026
 */
#pragma once
#include "utils/models/message.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <string>

class PacketBuilder {
public:
  // build a login packet
  static Packet buildLogin(const std::string &username, const std::string &password);

  // build a logout packet
  static Packet buildLogout(const User &user);

  // build a message packet
  static Packet buildMessage(const std::string &username, const Message &msg);

  // build a join room packet
  static Packet buildJoinRoom(const std::string &username, const std::string &room);

  // build a leave room packet
  static Packet buildLeaveRoom(const std::string &username, const std::string &room);

  // build a register packet
  static Packet buildRegister(const std::string &username, const std::string &password,
                              const std::string &email);
};