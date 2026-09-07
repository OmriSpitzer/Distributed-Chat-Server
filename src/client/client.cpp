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
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <any>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

namespace {
std::atomic<std::uint64_t> next_client_id{0};
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
  std::string username;
  if (state.user) {
    username = state.user->getUsername();
  }

  int answer = ui.showWelcome(username);
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
    } while (response && response->type == Packet::PacketType::HEARTBEAT);

    if (response && response->type == Packet::PacketType::LOGIN) {
      std::optional<std::any> result = handler.handlePacket(*response);
      if (result) {
        state.user = std::any_cast<User>(*result);
        state.loggedIn = true;
      }
    }

    break;
  }
  case 2: {
    std::optional<Packet> registerPacket = ui.showRegister();
    if (!registerPacket) {
      return;
    }
    network.sendPacket(*registerPacket, "Register request");
    break;
  }
  case 3: {
    stop();
    break;
  }
  default:
    break;
  }
}