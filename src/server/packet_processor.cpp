/**
 * PacketProcessor class implementation file
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response
 *
 * Used for processing incoming packets and building responses
 *
 * @date 13-09-2026
 */

#include "server/packet_processor.h"
#include "auth/authentication.h"
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/gossip_payload.h"
#include "utils/models/message.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <atomic>
#include <cstdint>
#include <ctime>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {
// next presence sequence
std::atomic<std::uint64_t> nextPresenceSeq{0};

// generate a unique event ID
std::string makeEventId(const std::string &kind, const std::string &username) {
  return config::NODE_ID + "-" + kind + "-" + username + "-" +
         std::to_string(static_cast<long long>(std::time(nullptr))) + "-" +
         std::to_string(++nextPresenceSeq);
}

// send an event to the network
void rumorEvent(ConnectionManager &connections, const std::string &type,
                const std::string &username, const std::string &content, const std::string &field5,
                const std::string &room = "") {
  // generate the event ID
  const std::string eventId = makeEventId(type, username);

  // encode the payload
  const std::string payload = gossip_payload::encode(type, eventId, username, content, field5);

  // create the event packet
  Packet event(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, room, payload);

  // send the event to the network
  connections.rumor(event);
}

// send a presence event to the network
void rumorPresence(ConnectionManager &connections, const char *kind, const std::string &username) {
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  rumorEvent(connections, kind, username, config::NODE_ID, ts);
}

// send a user created event to the network
void rumorUserCreated(ConnectionManager &connections, const std::string &username,
                      std::string_view passwordHash, std::string_view email) {
  rumorEvent(connections, "USER_CREATED", username, std::string(passwordHash), std::string(email));
}

// send a room join event to the network
void rumorRoomJoin(ConnectionManager &connections, const std::string &username,
                   const std::string &newRoom, const std::string &prevRoom) {
  rumorEvent(connections, "ROOM_JOIN", username, config::NODE_ID, prevRoom, newRoom);
}

// send a message event to the network
void rumorMessage(ConnectionManager &connections, const std::string &username,
                  const std::string &content, const std::string &room) {
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  rumorEvent(connections, "MESSAGE", username, content, ts, room);
}

// send a room created event to the network
void rumorRoomCreated(ConnectionManager &connections, const std::string &username,
                      const Room &room) {
  rumorEvent(connections, "ROOM_CREATED", username, Room::roomTypeToString(room.getType()),
             Room::privacyToString(room.getPrivacy()), room.getName());
}

// send an allow-list add event to the network
void rumorAclAdd(ConnectionManager &connections, const std::string &username,
                 const std::string &roomName, bool creator) {
  // code for if the user is the creator of the room
  std::string content = creator ? "1" : "0";
  rumorEvent(connections, "ROOM_ACL_ADD", username, content, "", roomName);
}

// send an allow-list remove event to the network
void rumorRoomKick(ConnectionManager &connections, const std::string &username,
                   const std::string &roomName) {
  rumorEvent(connections, "ROOM_KICK", username, config::NODE_ID, "", roomName);
}

// send a room deleted event to the network
void rumorRoomDeleted(ConnectionManager &connections, const std::string &username,
                      const std::string &roomName) {
  rumorEvent(connections, "ROOM_DELETED", username, config::NODE_ID, "", roomName);
}

// encode the current room list for clients
std::string encodeRoomDirectory() {
  return Room::serializeList(RoomManager::getInstance().listRooms());
}

// push the full room list to all local clients
void pushRoomDirectory(ConnectionManager &connections, SOCKET skipSocket = INVALID_SOCKET) {
  // create the room list packet
  Packet push("server", "*", Packet::PacketType::ROOM_LIST, "", encodeRoomDirectory(), 0);

  // broadcast the room list to all local clients
  RoomManager::getInstance().broadcastAll(push, connections, skipSocket);
}

// encode message history as a string. format: [user][timestamp] <content>
std::string encodeHistory(const std::vector<Message> &messages) {
  std::string out;
  for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
    out += "[" + it->getFrom().getUsername() + "][" + std::to_string(it->getTimestamp()) + "] " +
           it->getContent() + "\n";
  }
  return out;
}

// check if the user is an admin
bool isAdmin(const ClientSession &session) {
  return session.isAuthenticated() && session.getUser().getUserType() == User::UserType::ADMIN;
}

// deny the request
void denyForbidden(Packet &response, std::string_view message) {
  response.responseCode = static_cast<int>(RESPONSE_CODES::FORBIDDEN);
  response.message = std::string(message);
}

// find a session by username
ClientSession *findSessionByUsername(ConnectionManager &connections, const std::string &username) {
  for (const auto &entry : connections.getSessions()) {
    const auto &session = entry.second;
    if (session && session->isAuthenticated() && session->getUser().getUsername() == username) {
      return session.get();
    }
  }
  return nullptr;
}

// push the lobby to a specific client
void pushLobbyTo(ConnectionManager &connections, SOCKET socket) {
  // create the lobby packet
  Packet push("server", "*", Packet::PacketType::ROOM_LIST, RoomManager::LOBBY.getName(),
              encodeRoomDirectory(), 0);

  // send the lobby packet to the client
  connections.sendPacket(socket, push);
}
} // namespace

// process a packet
Packet PacketProcessor::processPacket(const Packet &packet, ClientSession &session,
                                      ConnectionManager &connections) {
  // create a response packet
  Packet response = Packet(packet);
  response.timestamp = static_cast<uint64_t>(std::time(nullptr));
  response.sender = "server";
  response.receiver = packet.sender;

  // process the packet
  switch (packet.type) {

    // heartbeat packet
  case Packet::PacketType::HEARTBEAT: {
    response.message = "pong";
    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    break;
  }

    // gossip packets
  case Packet::PacketType::GOSSIP_HELLO:
  case Packet::PacketType::GOSSIP_EVENT:
  case Packet::PacketType::GOSSIP_DIGEST:
  case Packet::PacketType::GOSSIP_PULL: {
    response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
    response.message = "unsupported on client port";
    break;
  }

  // room list packet
  case Packet::PacketType::ROOM_LIST: {
    response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
    response.message = "unsupported on client port";
    break;
  }

    // login packet
  case Packet::PacketType::LOGIN: {
    try {
      // get the user from the database
      std::variant<User, std::string> dbResult =
          DatabaseManager::getInstance().loginUser(packet.sender, packet.message);
      if (const auto *error = std::get_if<std::string>(&dbResult)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = *error;
        break;
      }

      // create the user object
      User user = std::get<User>(dbResult);

      // check if the user is already logged in
      if (connections.hasSession(user) ||
          DatabaseManager::getInstance().isUserOnline(user.getUsername())) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "user already logged in";
        break;
      }

      // get the previous room
      const std::string prevRoom = session.getRoom().getName();

      // set the session user and authenticated
      session.setUser(user);
      session.setAuthenticated(true);

      // join the lobby
      RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);

      // send the login event to the network
      rumorPresence(connections, "LOGIN", user.getUsername());

      // send the room join event to the network
      rumorRoomJoin(connections, user.getUsername(), RoomManager::LOBBY.getName(), prevRoom);

      // set the response code and message
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = user.serialize();
      response.room = encodeRoomDirectory();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("login failed");
    }
    break;
  }

    // register packet
  case Packet::PacketType::REGISTER: {
    try {
      // check if the user already exists
      if (DatabaseManager::getInstance().userExists(packet.sender)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "user already exists";
        break;
      }

      // parse the username, password and email
      std::string_view username = packet.sender;
      std::string_view password = packet.message;
      std::string_view email = packet.room;

      // check if the username, password and email are not empty
      if (username.empty() || password.empty() || email.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing username, password, or email";
        break;
      }

      // create the user
      const std::string hashedPassword = Authentication::hashPassword(password);
      User user = DatabaseManager::getInstance().createUser(username, hashedPassword, email);
      rumorUserCreated(connections, user.getUsername(), hashedPassword, email);

      // get the previous room
      const std::string prevRoom = session.getRoom().getName();

      // set the session user and authenticated
      session.setUser(user);
      session.setAuthenticated(true);

      // join the lobby
      RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);

      // send the login event to the network
      rumorPresence(connections, "LOGIN", user.getUsername());

      // send the room join event to the network
      rumorRoomJoin(connections, user.getUsername(), RoomManager::LOBBY.getName(), prevRoom);

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = user.serialize();
      response.room = encodeRoomDirectory();
    } catch (const DatabaseManager::ConstraintError &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = e.what();
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("register failed: ") + e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("register failed");
    }
    break;
  }

    // logout packet
  case Packet::PacketType::LOGOUT: {
    const bool wasAuth = session.isAuthenticated();               // get user authentication status
    const std::string username = session.getUser().getUsername(); // get username

    // leave all rooms
    RoomManager::getInstance().leaveAll(session);

    // set the session user and authenticated
    session.setAuthenticated(false);
    session.setUser(User::anonymousUser());

    // keep connected anonymous socket in Lobby for live broadcasts
    RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);

    // send the logout event to the network
    if (wasAuth && !username.empty()) {
      rumorPresence(connections, "LOGOUT", username);
    }

    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "logged out";
    break;
  }

  // message packet
  case Packet::PacketType::MESSAGE: {
    // check if the message is empty
    if (packet.message.empty()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "empty message";
      break;
    }

    try {
      const std::string username = session.getUser().getUsername(); // get username
      const Room room = session.getRoom();                          // get room

      // check if the user is authenticated
      if (session.isAuthenticated()) {
        rumorMessage(connections, username, packet.message, room.getName());
      } else {
        // if guest, broadcast the message to the room
        Packet push(username, "", Packet::PacketType::MESSAGE, room.getName(), packet.message, 0);
        RoomManager::getInstance().broadcast(room, push, connections);
      }

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "ok";
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("send failed: ") + e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("send failed");
    }
    break;
  }

    // room join packet
  case Packet::PacketType::ROOM_JOIN: {
    try {
      const std::string roomName =
          packet.room.empty() ? RoomManager::LOBBY.getName() : packet.room; // get the room name
      auto room = RoomManager::getInstance().getRoom(roomName);             // get the room

      // check if the room exists
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

      DatabaseManager &db = DatabaseManager::getInstance();
      const std::string email = session.getUser().getEmail();

      // check if the room is private and the user is not an admin
      if (room->getPrivacy() == Room::Privacy::PRIVATE && !isAdmin(session) &&
          (!session.isAuthenticated() || !db.isAllowed(room->getId(), email))) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "not allowed to join " + roomName;
        break;
      }

      // get the previous room
      const std::string prevRoom = session.getRoom().getName();

      // join the room
      if (!RoomManager::getInstance().joinRoom(roomName, session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to join " + roomName;
        break;
      }

      // add to allow-list for registered users in public rooms
      if (session.isAuthenticated() && room->getPrivacy() == Room::Privacy::PUBLIC) {
        db.addToAllowList(room->getId(), email, false);
      }

      // users are only rumored for joining rooms
      if (session.isAuthenticated()) {
        rumorRoomJoin(connections, session.getUser().getUsername(), roomName, prevRoom);
      }

      response.room = roomName;
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "joined " + roomName;
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("join failed: ") + e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("join failed");
    }
    break;
  }

    // room leave packet
  case Packet::PacketType::ROOM_LEAVE: {
    try {
      const std::string prevRoom = session.getRoom().getName(); // get the previous room

      // join the lobby
      if (!RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to leave room";
        break;
      }

      // send the room join event to the network
      if (session.isAuthenticated()) {
        rumorRoomJoin(connections, session.getUser().getUsername(), RoomManager::LOBBY.getName(),
                      prevRoom);
      }

      response.room = RoomManager::LOBBY.getName();
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = "left room, back in Lobby";
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("leave failed: ") + e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("leave failed");
    }
    break;
  }

    // create a room packet
  case Packet::PacketType::ROOM_CREATE: {
    // check if the user is authenticated
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string roomName = packet.room; // get the room name

      // check if the room name is empty
      if (roomName.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room name";
        break;
      }

      // privacy from packet.message (default PUBLIC)
      Room::Privacy privacy = Room::Privacy::PUBLIC;
      if (!packet.message.empty()) {
        privacy = Room::stringToPrivacy(packet.message);
      }

      // create the room and add to the database
      Room draft(0, roomName, Room::RoomType::OTHER, privacy);
      if (!RoomManager::getInstance().createRoom(draft, session.getUser().getEmail())) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "room already exists or create failed";
        break;
      }

      // get the created room
      auto created = RoomManager::getInstance().getRoom(roomName);

      // check if the room was created
      if (!created) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
        response.message = "create failed";
        break;
      }

      // send the room created event to the network
      rumorRoomCreated(connections, session.getUser().getUsername(), *created);

      // push the room list to all local clients
      pushRoomDirectory(connections);

      response.room = roomName;
      response.message = created->serialize();
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = "create failed";
    }
    break;
  }

    // user invite to room packet
  case Packet::PacketType::ROOM_INVITE: {
    // check if the user is authenticated
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string roomName =
          packet.room.empty() ? session.getRoom().getName() : packet.room; // get the room name
      const std::string inviteeUsername = packet.message; // get the invitee username

      // check if the room name or invitee username is empty
      if (roomName.empty() || inviteeUsername.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room or invitee username";
        break;
      }

      auto room = RoomManager::getInstance().getRoom(roomName); // get the room

      // check if the room exists
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

      // check if the room is private and the user is not an admin
      if (room->getPrivacy() != Room::Privacy::PRIVATE && !isAdmin(session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "invite only allowed for private rooms";
        break;
      }

      // check if the user is an admin or the room creator
      DatabaseManager &db = DatabaseManager::getInstance();
      if (!isAdmin(session) &&
          !db.isAllowListCreator(room->getId(), session.getUser().getEmail())) {
        denyForbidden(response, "only the room creator or an admin can invite");
        break;
      }

      // get the invitee user
      auto invitee = db.getUser(inviteeUsername);

      // check if the invitee user exists
      if (!invitee) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown user: " + inviteeUsername;
        break;
      }

      // add the invitee to the allow-list
      db.addToAllowList(room->getId(), invitee->getEmail(), false);

      // send the allow-list add event to the network
      rumorAclAdd(connections, inviteeUsername, roomName, false);

      response.room = roomName;
      response.message = "invited " + inviteeUsername;
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    } catch (const DatabaseManager::ConstraintError &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = "invite failed";
    }
    break;
  }

    // delete room packet
  case Packet::PacketType::ROOM_DELETE: {
    // check if the user is an admin
    if (!isAdmin(session)) {
      denyForbidden(response, "admin required");
      break;
    }

    try {
      const std::string roomName =
          packet.room.empty() ? session.getRoom().getName() : packet.room; // get the room name

      // check if the room name is empty
      if (roomName.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room name";
        break;
      }

      // check if the room name is the lobby or general
      if (roomName == RoomManager::LOBBY.getName() || roomName == RoomManager::GENERAL.getName()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "cannot delete Lobby or General";
        break;
      }

      // delete the room
      if (!RoomManager::getInstance().deleteRoom(roomName, connections)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "unknown room or delete failed";
        break;
      }

      // send the room deleted event to the network
      rumorRoomDeleted(connections, session.getUser().getUsername(), roomName);

      // push the room list to all local clients
      pushRoomDirectory(connections);

      response.room = RoomManager::LOBBY.getName();
      response.message = "deleted " + roomName;
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = "delete failed";
    }
    break;
  }

    // kick user from room packet
  case Packet::PacketType::ROOM_KICK: {
    // check if the user is authenticated
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string roomName =
          packet.room.empty() ? session.getRoom().getName() : packet.room; // get the room name
      const std::string targetUsername = packet.message; // get the target username

      // check if the room name or target username is empty
      if (roomName.empty() || targetUsername.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room or username";
        break;
      }

      // check if the room name is the lobby
      if (roomName == RoomManager::LOBBY.getName()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "cannot kick from Lobby";
        break;
      }

      // check if the target username is the same as the current user
      if (targetUsername == session.getUser().getUsername()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "cannot kick yourself";
        break;
      }

      // get the room
      auto room = RoomManager::getInstance().getRoom(roomName);

      // check if the room exists
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

      // check if the user is an admin or the room creator
      DatabaseManager &db = DatabaseManager::getInstance();
      if (!isAdmin(session) &&
          !db.isAllowListCreator(room->getId(), session.getUser().getEmail())) {
        denyForbidden(response, "only the room creator or an admin can kick");
        break;
      }

      // get the target user
      auto target = db.getUser(targetUsername);
      if (!target) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown user: " + targetUsername;
        break;
      }

      // remove the target user from the allow-list
      db.removeFromAllowList(room->getId(), target->getEmail());

      ClientSession *targetSession =
          findSessionByUsername(connections, targetUsername); // get the target session

      // check if the target user is in the room
      if (targetSession && targetSession->getRoom().getName() == roomName) {
        const std::string prevRoom = roomName; // get the previous room

        // join the lobby
        RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), *targetSession);

        // send the room join event to the network
        rumorRoomJoin(connections, targetUsername, RoomManager::LOBBY.getName(), prevRoom);

        // push the lobby to the target user
        pushLobbyTo(connections, targetSession->getSocket());
      }

      // send the room kick event to the network
      rumorRoomKick(connections, targetUsername, roomName);

      response.room = roomName;
      response.message = "kicked " + targetUsername;
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    } catch (const DatabaseManager::ConstraintError &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = "kick failed";
    }
    break;
  }

    // load message history packet
  case Packet::PacketType::LOAD_MESSAGE_HISTORY: {
    try {
      const std::string roomName =
          packet.room.empty() ? session.getRoom().getName() : packet.room; // get the room name

      // check if the room exists
      auto room = RoomManager::getInstance().getRoom(roomName);
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

      // load the message history
      const auto history = DatabaseManager::getInstance().loadHistory(room->getId());

      response.room = roomName;
      response.message = encodeHistory(history);
      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = std::string("load history failed: ") + e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = "load history failed";
    }
    break;
  }

    // update user packet
  case Packet::PacketType::UPDATE_USER: {
    // check if the user is authenticated
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string currentUsername =
          session.getUser().getUsername();                 // get the current username
      const std::string_view newUsername = packet.sender;  // get the new username
      const std::string_view newPassword = packet.message; // get the new password
      const std::string_view email = packet.room;          // get the email

      // check if the new username or email is empty
      if (newUsername.empty() || email.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing username or email";
        break;
      }

      // update the user
      User updated = DatabaseManager::getInstance().updateUser(currentUsername, newUsername,
                                                               newPassword, email);
      // set the updated user
      session.setUser(updated); // set the updated user

      response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
      response.message = updated.serialize();
    } catch (const DatabaseManager::ConstraintError &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = e.what();
    } catch (const std::exception &e) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = e.what();
    } catch (...) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
      response.message = "update failed";
    }
    break;
  }

  // unknown packet type
  default: {
    response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
    response.message = "unknown packet type";
    break;
  }
  }

  return response;
}