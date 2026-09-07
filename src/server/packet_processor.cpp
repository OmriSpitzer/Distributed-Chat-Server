/**
 * PacketProcessor class
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response.
 * @date 03-09-2026
 */

#include "server/packet_processor.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/models/user.h"
#include <any>
#include <exception>
#include <string>

// process a packet
Packet PacketProcessor::processPacket(const Packet &packet, ClientSession &session,
                                      ConnectionManager &connections) {
  // create a response packet
  Packet response = packet.copy();
  response.sender = "server";
  response.receiver = packet.sender;

  // process the packet
  switch (packet.type) {

    // login packet
  case Packet::PacketType::LOGIN: {
    try {
      // get the user from the database
      std::any dbResult = DatabaseManager::getInstance().getUser(packet.sender, packet.message);
      if (const auto *error = std::any_cast<std::string>(&dbResult)) {
        response.responseCode = 401;
        response.message = *error;
        break;
      }

      User user = std::any_cast<User>(dbResult);

      // check if there is a session of the same user
      if (connections.hasSession(user)) {
        response.responseCode = 401;
        response.message = "user already logged in";
        break;
      }

      session.setUser(user);
      session.setAuthenticated(true);

      response.responseCode = 200;
      response.message = user.serialize();
    } catch (const std::exception &e) {
      response.responseCode = 500;
      response.message = std::string("login failed: ") + e.what();
    }
    break;
  }

    // register packet
  case Packet::PacketType::REGISTER: {
    try {
      // get the user from the database
      std::any dbResult = DatabaseManager::getInstance().getUser(packet.sender, packet.message);
      if (const auto *user = std::any_cast<User>(&dbResult)) {
        response.responseCode = 401;
        response.message = "user already exists";
        break;
      }

      // parse the username, password and email
      std::string_view username = packet.sender;
      std::string_view password = packet.message.substr(0, packet.message.rfind('|'));
      std::string_view email = packet.message.substr(packet.message.rfind('|') + 1);

      // create the user
      User user = DatabaseManager::getInstance().createUser(username, password, email);

      // set the session
      session.setUser(user);
      session.setAuthenticated(true);

      response.responseCode = 200;
      response.message = user.serialize();
    } catch (const std::exception &e) {
      response.responseCode = 500;
      response.message = std::string("register failed: ") + e.what();
    }
    break;
  }

    // logout packet
  case Packet::PacketType::LOGOUT: {
    // initialize the session
    session.setAuthenticated(false);
    session.setUser(User::anonymousUser());
    session.setRoom(RoomManager::LOBBY);

    response.responseCode = 200;
    response.message = "logged out";
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

  case Packet::PacketType::HEARTBEAT:
    response.message = "pong";
    break;
  }

  return response;
}

// process a heartbeat packet
Packet PacketProcessor::processHeartbeatPacket(const Packet &packet,
                                               ConnectionManager &connections) {
  auto sessions = connections.getSessions();

  std::string message = "sessions: ";
  for (const auto &entry : sessions) {
    const ClientSession &session = *entry.second;
    message += "[" + session.getUser().getUsername() + "," + session.getRoom().getName() + "] ";
  }
  message += "total: " + std::to_string(sessions.size());

  Packet response = packet.copy();
  response.sender = "server";
  response.receiver = packet.sender;
  response.message = message;
  return response;
}
