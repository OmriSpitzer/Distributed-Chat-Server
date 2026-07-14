/**
 * PacketProcessor class
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response.
 * @date 14-07-2026
 */

#include "server/packet_processor.h"
#include <ctime>
#include <exception>

Packet PacketProcessor::processPacket(const Packet &packet, ClientSession &session) {
  Packet response;
  response.sender = "server";
  response.receiver = packet.sender;
  response.room = packet.room;
  response.timestamp = static_cast<uint64_t>(std::time(nullptr));
  response.type = packet.type;

  switch (packet.type) {
  case Packet::PacketType::LOGIN: {
    try {
      User user = Authentication::login(packet.sender, packet.message);
      session.setUser(user);
      session.setAuthenticated(true);
      response.message = "login ok";
    } catch (const std::exception &e) {
      response.message = std::string("login failed: ") + e.what();
    }
    break;
  }
  case Packet::PacketType::LOGOUT: {
    Authentication::logout(packet.sender);
    session.setAuthenticated(false);
    response.message = "logout ok";
    break;
  }
  case Packet::PacketType::MESSAGE: {
    response.message = session.isAuthenticated() ? "message delivered" : "not authenticated";
    break;
  }
  case Packet::PacketType::ROOM_JOIN: {
    response.message = "joined " + packet.room;
    break;
  }
  case Packet::PacketType::ROOM_LEAVE: {
    response.message = "left " + packet.room;
    break;
  }
  }

  return response;
}
