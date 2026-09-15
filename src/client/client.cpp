/**
 * Client class
 *
 * @brief Wires together the client components and runs a basic login flow.
 * @date 13-09-2026
 */

#include "client/client.h"
#include "client/console_ui.h"
#include "client/packet_builder.h"
#include "client/packet_handler.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace {
std::atomic<std::uint64_t> next_client_id{0};
} // namespace

// waiting for a packet of a specific type
std::optional<Packet> Client::waitFor(Packet::PacketType expected) {
  while (true) {
    auto response = network.receivePacket();
    if (!response) {
      Logger::logError("Client " + id, "Connection lost while waiting for response");
      return std::nullopt;
    }
    if (response->type == expected) {
      return response;
    }
    // ignore unexpected queued types (heartbeats/pushes are handled in Network)
  }
}

// apply a room directory payload (login uses room field; pushes use message)
void Client::applyRoomDirectory(const std::string &encoded) {
  try {
    state.setRooms(Room::deserializeList(encoded));
  } catch (const std::exception &e) {
    Logger::logWarning("Client " + id, std::string("Bad room directory: ") + e.what());
  }
}

// start the client
bool Client::start() {
  id = std::to_string(++next_client_id);

  network.setPushHandler([this](const Packet &packet) {
    if (packet.type == Packet::PacketType::ROOM_LIST) {
      applyRoomDirectory(packet.message);
    }
  });

  // connect to the server
  if (!network.connect()) {
    Logger::logError("Client " + id, "Failed to connect to the server");
    return false;
  }

  return true;
}

// stop the client
void Client::stop() { network.disconnect(); }

// check if the client is alive
bool Client::isAlive() const { return network.isConnected(); }

// showing the dashboard
void Client::showDashboard() {
  if (!state.isLoggedIn()) {
    // show home screen
    int answer = ConsoleUI::showHomeScreen();

    switch (answer) {
      // login option
    case 1: {
      std::optional<Packet> loginPacket = ConsoleUI::showLogin();
      if (!loginPacket) {
        return;
      }
      if (!network.sendPacket(*loginPacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::LOGIN);
      if (!response) {
        break;
      }

      if (auto user = handler.handlePacket(*response)) {
        state.user = *user;
        state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
        applyRoomDirectory(response->room);
      } else if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        Logger::logError("Client " + id, "Login failed: " + response->message);
      } else {
        Logger::logError("Client " + id, "Login failed: invalid user payload");
      }

      break;
    }

    // register option
    case 2: {
      std::optional<Packet> registerPacket = ConsoleUI::showRegister();
      if (!registerPacket) {
        return;
      }
      if (!network.sendPacket(*registerPacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::REGISTER);
      if (!response) {
        break;
      }

      if (auto user = handler.handlePacket(*response)) {
        state.user = *user;
        state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
        applyRoomDirectory(response->room);
      } else if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        Logger::logError("Client " + id, "Register failed: " + response->message);
      } else {
        Logger::logError("Client " + id, "Register failed: invalid user payload");
      }

      break;
    }

    // exit option
    case 3: {
      stop();
      break;
    }
    default:
      break;
    }
  } else {
    // show user dashboard
    int answer = ConsoleUI::showUserDashboard(state);

    switch (answer) {
      // update profile option
    case 1: {
      std::optional<Packet> updatePacket = ConsoleUI::showUpdateProfile(*state.user);
      if (!updatePacket) {
        break;
      }

      if (!network.sendPacket(*updatePacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::UPDATE_USER);
      if (!response) {
        break;
      }

      if (auto user = handler.handlePacket(*response)) {
        state.user = *user;
        Logger::logInfo("Client " + id, "Profile updated: " + user->getUsername());
      } else if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        Logger::logError("Client " + id, "Update failed: " + response->message);
      } else {
        Logger::logError("Client " + id, "Update failed: invalid user payload");
      }
      break;
    }

    // join room option
    case 2: {
      std::optional<Packet> joinRoomPacket = ConsoleUI::showJoinRoom(state);

      if (!joinRoomPacket) {
        break;
      }

      if (!network.sendPacket(*joinRoomPacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::ROOM_JOIN);
      if (!response) {
        break;
      }

      if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        state.currentRoom = Room(0, response->room);
        // TODO: change visuals
      } else {
        Logger::logError("Client " + id, "Join failed: " + response->message);
      }
      break;
    }

    // leave room option
    case 3: {
      if (!state.currentRoom) {
        break;
      }
      if (state.currentRoom->getType() == Room::RoomType::LOBBY) {
        Logger::logError("Client " + id, "Cannot leave the Lobby");
        break;
      }

      std::string roomName = state.currentRoom->getName();
      std::string username = state.user->getUsername();

      if (roomName.empty()) {
        break;
      }

      Packet leaveRoomPacket = PacketBuilder::buildLeaveRoom(username, roomName);
      if (!network.sendPacket(leaveRoomPacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::ROOM_LEAVE);
      if (!response) {
        break;
      }

      if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
        Logger::logInfo("Client " + id, "Left room " + roomName);
      } else {
        Logger::logError("Client " + id, "Failed to leave room " + roomName);
      }
      break;
    }

    // create message option
    case 4: {
      std::optional<Packet> createMessagePacket = ConsoleUI::showCreateMessage(*state.user);

      if (!createMessagePacket) {
        break;
      }

      if (!network.sendPacket(*createMessagePacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::MESSAGE);
      if (!response) {
        break;
      }

      if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        Logger::logInfo("Client " + id, "Message sent successfully");
      } else {
        Logger::logError("Client " + id, "Failed to send message");
      }
      break;
    }

    // create room option
    case 5: {
      std::optional<Packet> createRoomPacket = ConsoleUI::showCreateRoom(*state.user);
      if (!createRoomPacket) {
        break;
      }

      if (!network.sendPacket(*createRoomPacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::ROOM_CREATE);
      if (!response) {
        break;
      }

      if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        Logger::logInfo("Client " + id, "Room created: " + response->room);
      } else {
        Logger::logError("Client " + id, "Create room failed: " + response->message);
      }
      break;
    }

    // load message history option
    case 6: {
      if (!state.user || !state.currentRoom) {
        break;
      }

      const std::string roomName = state.currentRoom->getName();
      if (roomName.empty()) {
        break;
      }

      Packet historyPacket;
      try {
        historyPacket =
            PacketBuilder::buildLoadMessageHistory(state.user->getUsername(), roomName);
      } catch (const std::invalid_argument &e) {
        Logger::logError("Client " + id, e.what());
        break;
      }

      if (!network.sendPacket(historyPacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::LOAD_MESSAGE_HISTORY);
      if (!response) {
        break;
      }

      if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        std::cout << ">> History for " << response->room << ":\n";
        if (response->message.empty()) {
          std::cout << "   (no messages)\n";
        } else {
          std::cout << response->message;
        }
      } else {
        Logger::logError("Client " + id, "Load history failed: " + response->message);
      }
      break;
    }

    // invite to room option
    case 7: {
      std::optional<Packet> invitePacket = ConsoleUI::showInviteToRoom(state);
      if (!invitePacket) {
        break;
      }

      if (!network.sendPacket(*invitePacket)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::ROOM_INVITE);
      if (!response) {
        break;
      }

      if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        Logger::logInfo("Client " + id, response->message);
      } else {
        Logger::logError("Client " + id, "Invite failed: " + response->message);
      }
      break;
    }

    // logout option
    case 8: {
      Packet logout = PacketBuilder::buildLogout(*state.user);
      if (!network.sendPacket(logout)) {
        break;
      }

      auto response = waitFor(Packet::PacketType::LOGOUT);
      if (!response) {
        break;
      }

      if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
        state.clear();
      } else {
        Logger::logError("Client " + id, "Logout failed: " + response->message);
      }
      break;
    }
    default:
      break;
    }
  }
}
