/**
 * PacketProcessor class
 *
 * @brief Routes incoming packets to the appropriate manager and builds a response.
 * @date 13-09-2026
 */

#include "server/packet_processor.h"
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
std::atomic<std::uint64_t> nextPresenceSeq{0};

// generate a unique event id
std::string makeEventId(const std::string &kind, const std::string &username) {
  return config::NODE_ID + "-" + kind + "-" + username + "-" +
         std::to_string(static_cast<long long>(std::time(nullptr))) + "-" +
         std::to_string(++nextPresenceSeq);
}

// send an event to the network
void rumorEvent(ConnectionManager &connections, const std::string &type,
                const std::string &username, const std::string &content, const std::string &field5,
                const std::string &room = "") {
  const std::string eventId = makeEventId(type, username);
  const std::string payload = gossip_payload::encode(type, eventId, username, content, field5);
  Packet event(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, room, payload);
  connections.rumor(event);
}

// send a presence event to the network
void rumorPresence(ConnectionManager &connections, const char *kind, const std::string &username) {
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  rumorEvent(connections, kind, username, config::NODE_ID, ts);
}

// send a user created event to the network
void rumorUserCreated(ConnectionManager &connections, const std::string &username,
                      std::string_view password, std::string_view email) {
  rumorEvent(connections, "USER_CREATED", username, std::string(password), std::string(email));
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

// replicate an allow-list add (invitee username; content "1" = creator)
void rumorAclAdd(ConnectionManager &connections, const std::string &username,
                 const std::string &roomName, bool creator) {
  rumorEvent(connections, "ROOM_ACL_ADD", username, creator ? "1" : "0", "", roomName);
}

// replicate an allow-list remove + force-leave (kick)
void rumorRoomKick(ConnectionManager &connections, const std::string &username,
                   const std::string &roomName) {
  rumorEvent(connections, "ROOM_KICK", username, config::NODE_ID, "", roomName);
}

// replicate a room delete
void rumorRoomDeleted(ConnectionManager &connections, const std::string &username,
                      const std::string &roomName) {
  rumorEvent(connections, "ROOM_DELETED", username, config::NODE_ID, "", roomName);
}

// encode the current room directory for clients
std::string encodeRoomDirectory() {
  return Room::serializeList(RoomManager::getInstance().listRooms());
}

// push the full room directory to all local clients
void pushRoomDirectory(ConnectionManager &connections, SOCKET skipSocket = INVALID_SOCKET) {
  Packet push("server", "*", Packet::PacketType::ROOM_LIST, "", encodeRoomDirectory(), 0);
  RoomManager::getInstance().broadcastAll(push, connections, skipSocket);
}

// encode history newest-first DB rows as oldest-first lines: [user][unix] text
std::string encodeHistory(const std::vector<Message> &messages) {
  std::string out;
  for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
    out += "[" + it->getFrom().getUsername() + "][" + std::to_string(it->getTimestamp()) + "] " +
           it->getContent() + "\n";
  }
  return out;
}

bool isAdmin(const ClientSession &session) {
  return session.isAuthenticated() &&
         session.getUser().getUserType() == User::UserType::ADMIN;
}

void denyForbidden(Packet &response, std::string_view message) {
  response.responseCode = static_cast<int>(RESPONSE_CODES::FORBIDDEN);
  response.message = std::string(message);
}

ClientSession *findSessionByUsername(ConnectionManager &connections, const std::string &username) {
  for (const auto &entry : connections.getSessions()) {
    const auto &session = entry.second;
    if (session && session->isAuthenticated() && session->getUser().getUsername() == username) {
      return session.get();
    }
  }
  return nullptr;
}

void pushLobbyTo(ConnectionManager &connections, SOCKET socket) {
  Packet push("server", "*", Packet::PacketType::ROOM_LIST, RoomManager::LOBBY.getName(),
              encodeRoomDirectory(), 0);
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

      User user = std::get<User>(dbResult);

      // local socket or another node's presence row
      if (connections.hasSession(user) ||
          DatabaseManager::getInstance().isUserOnline(user.getUsername())) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "user already logged in";
        break;
      }

      const std::string prevRoom = session.getRoom().getName();
      session.setUser(user);
      session.setAuthenticated(true);
      RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);
      rumorPresence(connections, "LOGIN", user.getUsername());
      rumorRoomJoin(connections, user.getUsername(), RoomManager::LOBBY.getName(), prevRoom);

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
      User user = DatabaseManager::getInstance().createUser(username, password, email);
      rumorUserCreated(connections, user.getUsername(), password, email);

      // set the session
      const std::string prevRoom = session.getRoom().getName();
      session.setUser(user);
      session.setAuthenticated(true);
      RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);
      rumorPresence(connections, "LOGIN", user.getUsername());
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
    const bool wasAuth = session.isAuthenticated();
    const std::string username = session.getUser().getUsername();

    RoomManager::getInstance().leaveAll(session);
    session.setAuthenticated(false);
    session.setUser(User::anonymousUser());
    // keep connected anonymous socket in Lobby for live broadcasts
    RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session);

    if (wasAuth && !username.empty()) {
      rumorPresence(connections, "LOGOUT", username);
    }

    response.responseCode = static_cast<int>(RESPONSE_CODES::SUCCESS);
    response.message = "logged out";
    break;
  }

  // message packet — registered users rumor+persist; guests broadcast in-memory only
  case Packet::PacketType::MESSAGE: {
    if (packet.message.empty()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "empty message";
      break;
    }

    try {
      const std::string username = session.getUser().getUsername();
      const Room room = session.getRoom();

      if (session.isAuthenticated()) {
        rumorMessage(connections, username, packet.message, room.getName());
      } else {
        // guests are not in users — skip DB persist / gossip FK; local room push only
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
      const std::string roomName = packet.room.empty() ? RoomManager::LOBBY.getName() : packet.room;
      auto room = RoomManager::getInstance().getRoom(roomName);
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

      DatabaseManager &db = DatabaseManager::getInstance();
      const std::string email = session.getUser().getEmail();
      if (room->getPrivacy() == Room::Privacy::PRIVATE && !isAdmin(session)) {
        if (!session.isAuthenticated() || !db.isAllowed(room->getId(), email)) {
          response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
          response.message = "not allowed to join " + roomName;
          break;
        }
      }

      const std::string prevRoom = session.getRoom().getName();
      if (!RoomManager::getInstance().joinRoom(roomName, session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to join " + roomName;
        break;
      }

      // only persist allow-list for registered users
      if (session.isAuthenticated() && room->getPrivacy() == Room::Privacy::PUBLIC) {
        db.addToAllowList(room->getId(), email, false);
      }

      // guests are in-memory only — membership FK requires a registered username
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

    // room leave packet — returns to Lobby; membership replica via ROOM_JOIN
  case Packet::PacketType::ROOM_LEAVE: {
    try {
      const std::string prevRoom = session.getRoom().getName();
      if (!RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "failed to leave room";
        break;
      }

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

    // create a room, then push the full directory to local clients
  case Packet::PacketType::ROOM_CREATE: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string roomName = packet.room;
      if (roomName.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room name";
        break;
      }

      auto privacy = Room::Privacy::PUBLIC;
      const auto type = Room::RoomType::OTHER;

      if (!packet.message.empty()) {
        privacy = Room::stringToPrivacy(packet.message);
      }

      Room draft(0, roomName, type, privacy);
      if (!RoomManager::getInstance().createRoom(draft, session.getUser().getEmail())) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "room already exists or create failed";
        break;
      }

      auto created = RoomManager::getInstance().getRoom(roomName);
      if (!created) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::INTERNAL_SERVER_ERROR);
        response.message = "create failed";
        break;
      }

      rumorRoomCreated(connections, session.getUser().getUsername(), *created);
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

    // invite a user to a private room (creator only)
  case Packet::PacketType::ROOM_INVITE: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string roomName = packet.room.empty() ? session.getRoom().getName() : packet.room;
      const std::string inviteeUsername = packet.message;
      if (roomName.empty() || inviteeUsername.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room or invitee username";
        break;
      }

      auto room = RoomManager::getInstance().getRoom(roomName);
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }
      if (room->getPrivacy() != Room::Privacy::PRIVATE && !isAdmin(session)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "invite only allowed for private rooms";
        break;
      }

      DatabaseManager &db = DatabaseManager::getInstance();
      if (!isAdmin(session) && !db.isAllowListCreator(room->getId(), session.getUser().getEmail())) {
        denyForbidden(response, "only the room creator or an admin can invite");
        break;
      }

      auto invitee = db.getUser(inviteeUsername);
      if (!invitee) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown user: " + inviteeUsername;
        break;
      }

      db.addToAllowList(room->getId(), invitee->getEmail(), false);
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

    // delete a room (ADMIN only; Lobby / General refused by RoomManager)
  case Packet::PacketType::ROOM_DELETE: {
    if (!isAdmin(session)) {
      denyForbidden(response, "admin required");
      break;
    }

    try {
      const std::string roomName = packet.room.empty() ? session.getRoom().getName() : packet.room;
      if (roomName.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room name";
        break;
      }
      if (roomName == RoomManager::LOBBY.getName() || roomName == RoomManager::GENERAL.getName()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "cannot delete Lobby or General";
        break;
      }

      if (!RoomManager::getInstance().deleteRoom(roomName, connections)) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "unknown room or delete failed";
        break;
      }

      rumorRoomDeleted(connections, session.getUser().getUsername(), roomName);
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

    // kick a user from a room (ADMIN, or allow-list creator)
  case Packet::PacketType::ROOM_KICK: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string roomName = packet.room.empty() ? session.getRoom().getName() : packet.room;
      const std::string targetUsername = packet.message;
      if (roomName.empty() || targetUsername.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing room or username";
        break;
      }
      if (roomName == RoomManager::LOBBY.getName()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "cannot kick from Lobby";
        break;
      }
      if (targetUsername == session.getUser().getUsername()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "cannot kick yourself";
        break;
      }

      auto room = RoomManager::getInstance().getRoom(roomName);
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

      DatabaseManager &db = DatabaseManager::getInstance();
      if (!isAdmin(session) && !db.isAllowListCreator(room->getId(), session.getUser().getEmail())) {
        denyForbidden(response, "only the room creator or an admin can kick");
        break;
      }

      auto target = db.getUser(targetUsername);
      if (!target) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown user: " + targetUsername;
        break;
      }

      db.removeFromAllowList(room->getId(), target->getEmail());

      if (ClientSession *targetSession = findSessionByUsername(connections, targetUsername)) {
        if (targetSession->getRoom().getName() == roomName) {
          const std::string prevRoom = roomName;
          RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), *targetSession);
          rumorRoomJoin(connections, targetUsername, RoomManager::LOBBY.getName(), prevRoom);
          pushLobbyTo(connections, targetSession->getSocket());
        }
      }

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

    // room list is server-push only on the client port
  case Packet::PacketType::ROOM_LIST: {
    response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
    response.message = "unsupported on client port";
    break;
  }

    // load recent message history for a room (guests allowed — read-only)
  case Packet::PacketType::LOAD_MESSAGE_HISTORY: {
    try {
      const std::string roomName = packet.room.empty() ? session.getRoom().getName() : packet.room;
      auto room = RoomManager::getInstance().getRoom(roomName);
      if (!room) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::NOT_FOUND);
        response.message = "unknown room: " + roomName;
        break;
      }

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

    // update profile (username and/or password); email must match the session user
  case Packet::PacketType::UPDATE_USER: {
    if (!session.isAuthenticated()) {
      response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
      response.message = "not authenticated";
      break;
    }

    try {
      const std::string currentUsername = session.getUser().getUsername();
      const std::string_view newUsername = packet.sender;
      const std::string_view newPassword = packet.message;
      const std::string_view email = packet.room;

      if (newUsername.empty() || email.empty()) {
        response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
        response.message = "missing username or email";
        break;
      }

      User updated = DatabaseManager::getInstance().updateUser(currentUsername, newUsername,
                                                               newPassword, email);
      session.setUser(updated);

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

  default: {
    response.responseCode = static_cast<int>(RESPONSE_CODES::ERROR);
    response.message = "unknown packet type";
    break;
  }
  }

  return response;
}