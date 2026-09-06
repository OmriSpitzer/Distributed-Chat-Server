/**
 * Client class
 *
 * @brief Wires together the client components and runs a basic login flow.
 * @date 06-09-2026
 */

#include "client/client.h"
#include "client/console_ui.h"
#include "client/packet_builder.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include <atomic>
#include <cstdint>
#include <iostream>
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
  int answer = ui.showWelcome();
  switch (answer) {
  case 1: {
    std::optional<Packet> loginPacket = ui.showLogin();
    if (!loginPacket) {
      return;
    }
    std::cout << loginPacket->serialize() << std::endl;
    break;
  }
  case 2: {
    std::optional<Packet> registerPacket = ui.showRegister();
    if (!registerPacket) {
      return;
    }
    std::cout << registerPacket->serialize() << std::endl;
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