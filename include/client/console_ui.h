/**
 * ConsoleUI header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "client/client_state.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <optional>

class ConsoleUI {
public:
  // showing the home screen
  static int showHomeScreen();

  // showing user dashboard
  static int showUserDashboard(const ClientState &state);

  // showing the login screen; empty optional means the user went back
  static std::optional<Packet> showLogin();

  // showing the register screen
  static std::optional<Packet> showRegister();

  // showing the join room screen
  static std::optional<Packet> showJoinRoom(const ClientState &state);

  // showing the create room screen
  static std::optional<Packet> showCreateRoom(const User &user);

  // showing the create message screen
  static std::optional<Packet> showCreateMessage(const User &user);

  // showing the update profile screen
  static std::optional<Packet> showUpdateProfile(const User &user);
};
