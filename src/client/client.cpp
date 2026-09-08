/**
 * Client class
 *
 * @brief Wires together the client components and runs a basic login flow.
 * @date 07-09-2026
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
#include <any>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace {
std::atomic<std::uint64_t> next_client_id{0};

// Room broadcast push: MESSAGE with responseCode 0 (ack uses SUCCESS=200).
bool isChatPush(const Packet &packet) {
  return packet.type == Packet::PacketType::MESSAGE && packet.responseCode == 0;
}
}

// start the client
bool Client::start() {
  id = std::to_string(++next_client_id);

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
  if (!state.loggedIn || !state.user) {
    int answer = ui.showHomeScreen();
    switch (answer) {
    case 1: {
      std::optional<Packet> loginPacket = ui.showLogin();
      if (!loginPacket) {
        return;
      }
      if (!network.sendPacket(*loginPacket, "Login request")) {
        break;
      }

      // receive the login response, skipping heartbeat packets
      std::optional<Packet> response;
      do {
        response = network.receivePacket();
      } while (response && (response->type == Packet::PacketType::HEARTBEAT ||
                            response->type == Packet::PacketType::MESSAGE));

      if (response && response->type == Packet::PacketType::LOGIN) {
        std::optional<std::any> result = handler.handlePacket(*response);
        if (result) {
          state.user = std::any_cast<User>(*result);
          state.loggedIn = true;
          state.currentRoom = Room("Lobby", Room::RoomType::LOBBY);
        }
      }

      break;
    }
    case 2: {
      std::optional<Packet> registerPacket = ui.showRegister();
      if (!registerPacket) {
        return;
      }
      if (!network.sendPacket(*registerPacket, "Register request")) {
        break;
      }

      // receive the login response, skipping heartbeat packets
      std::optional<Packet> response;
      do {
        response = network.receivePacket();
      } while (response && (response->type == Packet::PacketType::HEARTBEAT ||
                            response->type == Packet::PacketType::MESSAGE));

      if (response && response->type == Packet::PacketType::REGISTER) {
        std::optional<std::any> result = handler.handlePacket(*response);
        if (result) {
          state.user = std::any_cast<User>(*result);
          state.loggedIn = true;
          state.currentRoom = Room("Lobby", Room::RoomType::LOBBY);
        }
      }

      break;
    }
    case 3: {
      stop();
      break;
    }
    default:
      break;
    }
  } else {
    int answer = ui.showUserDashboard(state);
    switch (answer) {
    case 1: {
      // TODO: update profile
      break;
    }
    case 2: {
      std::optional<Packet> joinRoomPacket = ui.showJoinRoom(*state.user);

      if (!joinRoomPacket) {
        break;
      }

      // send the join room request
      if (!network.sendPacket(*joinRoomPacket, "Join room request")) {
        break;
      }

      // wait for ROOM_JOIN; print any chat pushes that arrive first
      std::optional<Packet> response;
      do {
        response = network.receivePacket();
        if (response && isChatPush(*response)) {
          handler.handlePacket(*response);
        }
      } while (response && (response->type == Packet::PacketType::HEARTBEAT || isChatPush(*response)));

      if (response && response->type == Packet::PacketType::ROOM_JOIN) {
        if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
          state.currentRoom = Room(response->room);

          // TODO: change visuals
        }
      }
      break;
    }
    case 3: {

      // check if the user is in the Lobby
      if (state.currentRoom->getType() == Room::RoomType::LOBBY) {
        std::cout << ">> Cannot leave the Lobby" << std::endl;
        break;
      }

      std::string roomName = state.currentRoom->getName();
      std::string username = state.user->getUsername();

      if (roomName.empty()) {
        break;
      }

      // build the leave room packet
      std::optional<Packet> leaveRoomPacket = PacketBuilder::buildLeaveRoom(username, roomName);

      // send the leave room request
      if (!network.sendPacket(*leaveRoomPacket, "Leave room request")) {
        break;
      }

      // wait for ROOM_LEAVE; print any chat pushes that arrive first
      std::optional<Packet> response;
      do {
        response = network.receivePacket();
        if (response && isChatPush(*response)) {
          handler.handlePacket(*response);
        }
      } while (response && (response->type == Packet::PacketType::HEARTBEAT || isChatPush(*response)));

      if (response && response->type == Packet::PacketType::ROOM_LEAVE) {
        if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
          state.currentRoom = Room("Lobby", Room::RoomType::LOBBY);

          std::cout << ">> Left room " << roomName << std::endl;
        } else {
          std::cout << ">> Failed to leave room " << roomName << std::endl;
        }
      }
      break;
    }
    case 4: {
      std::optional<Packet> createMessagePacket = ui.showCreateMessage(*state.user);

      if (!createMessagePacket) {
        break;
      }

      // send the create message request
      if (!network.sendPacket(*createMessagePacket, "Create message request")) {
        break;
      }

      // wait for server MESSAGE ack; print peer pushes that arrive first
      std::optional<Packet> response;
      do {
        response = network.receivePacket();
        if (response && isChatPush(*response)) {
          handler.handlePacket(*response);
        }
      } while (response &&
               (response->type == Packet::PacketType::HEARTBEAT || isChatPush(*response)));

      if (response && response->type == Packet::PacketType::MESSAGE) {
        if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
          std::cout << ">> Message sent successfully" << std::endl;
        } else {
          std::cout << ">> Failed to send message" << std::endl;
        }
      }
      break;
    }
    case 5: {
      std::optional<Packet> logout = PacketBuilder::buildLogout(*state.user);

      if (!network.sendPacket(*logout, "Logout request")) {
        break;
      }

      // wait for LOGOUT; print any chat pushes that arrive first
      std::optional<Packet> response;
      do {
        response = network.receivePacket();
        if (response && isChatPush(*response)) {
          handler.handlePacket(*response);
        }
      } while (response && (response->type == Packet::PacketType::HEARTBEAT || isChatPush(*response)));

      if (response && response->type == Packet::PacketType::LOGOUT) {
        if (response->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS)) {
          state.user.reset();
          state.loggedIn = false;
          state.currentRoom.reset();
        }
      }
      break;
    }
    default:
      break;
    }
  }
}