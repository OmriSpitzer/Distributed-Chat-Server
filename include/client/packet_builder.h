/**
 * PacketBuilder header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "utils/models/message.h"
#include "utils/models/packet.h"
#include <string>

class PacketBuilder {
public:
  // building a login packet
  static Packet buildLogin(const std::string &username, const std::string &password);

  // building a message packet
  static Packet buildMessage(const Message &msg);

  // building a join room packet
  static Packet buildJoinRoom(const std::string &room);
};