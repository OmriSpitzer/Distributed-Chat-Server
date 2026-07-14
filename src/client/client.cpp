/**
 * Client class
 *
 * @brief Wires together the client components and runs a basic login flow.
 * @date 14-07-2026
 */

#include "client/client.h"
#include "client/console_ui.h"
#include "client/packet_builder.h"
#include <iostream>

void Client::start() {
  ConsoleUI::showLogin();
  if (!network.connect()) {
    ConsoleUI::showError();
    return;
  }
  Packet login = PacketBuilder::buildLogin("alice", "secret");
  network.sendPacket(login);
  Packet response = network.receivePacket();
  handler.handleLogin(response);
}

void Client::stop() { network.disconnect(); }
