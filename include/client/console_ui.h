/**
 * ConsoleUI header file class
 *
 * @date 06-09-2026
 */
#pragma once

#include "utils/models/packet.h"
#include <optional>
#include <string_view>

class ConsoleUI {
public:
  // showing the welcome screen
  static int showWelcome(const std::string_view username);

  // showing the login screen; empty optional means the user went back
  static std::optional<Packet> showLogin();

  // showing the register screen
  static std::optional<Packet> showRegister();
};
