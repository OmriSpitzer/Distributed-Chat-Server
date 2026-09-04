/**
 * PacketProcessor class
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response.
 * @date 03-09-2026
 */

#include "server/packet_processor.h"
#include "auth/authentication.h"
#include <exception>

// process a packet
Packet PacketProcessor::processPacket(const Packet &packet, ClientSession &session) {
  // create a response packet
  Packet response = packet.copy();
  response.sender = "server";
  response.receiver = packet.sender;

  // process the packet
  switch (packet.type) {

    // login packet
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

    // logout packet
  case Packet::PacketType::LOGOUT: {
    Authentication::logout(packet.sender);
    session.setAuthenticated(false);
    response.message = "logout ok";
    break;
  }

    // message packet
  case Packet::PacketType::MESSAGE: {

    response.message = session.isAuthenticated() ? "message delivered" : "not authenticated";
    break;
  }

    // room join packet
  case Packet::PacketType::ROOM_JOIN: {
    response.message = "joined " + packet.room;
    break;
  }

    // room leave packet
  case Packet::PacketType::ROOM_LEAVE: {
    response.message = "left " + packet.room;
    break;
  }
  }

  return response;
}
