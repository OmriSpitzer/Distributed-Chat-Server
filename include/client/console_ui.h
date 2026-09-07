/**
 * ConsoleUI header file class
 *
 * @date 07-09-2026
 */
#pragma once

#include "client/client_state.h"
#include "utils/models/packet.h"
#include <optional>
#include <string_view>

class ConsoleUI {
public:
  // showing the welcome screen
  static int showHomeScreen();

  // showing user dashboard
  static int showUserDashboard(const ClientState &state);

  // showing the login screen; empty optional means the user went back
  static std::optional<Packet> showLogin();

  // showing the register screen
  static std::optional<Packet> showRegister();
};
