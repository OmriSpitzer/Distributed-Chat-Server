/**
 * Client class
 *
 * @brief Wires together the client components and runs a basic login flow.
 * @date 14-07-2026
 */

#include "client/client.h"
#include "client/console_ui.h"
#include "client/packet_builder.h"
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>

namespace {
std::atomic<std::uint64_t> next_client_id{0};
}

// start the client
void Client::start() {
  id = std::to_string(++next_client_id);

  // connect to the server
  if (!network.connect()) {
    Logger::logError("Client " + id, "Failed to connect to the server");
    return;
  }

  // show the welcome screen
  int choice = -1;
  do {
    choice = ConsoleUI::showWelcome();
    switch (choice) {
    case 1:

      break;
    case 2:

      break;
    }
  } while (choice != 3);
}

// stop the client
void Client::stop() { network.disconnect(); }
