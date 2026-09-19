/**
 * ConsoleUI class
 *
 * @brief Simple stdout-based user interface.
 * @date 13-09-2026
 */

#include "client/console_ui.h"
#include "client/client_state.h"
#include "client/packet_builder.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
const int MIN_MENU_CHOICE = 1; // minimum menu choice
const int MAX_MENU_CHOICE = 3; // maximum menu choice
const int MIN_USER_CHOICE = 1; // minimum user dashboard choice
const int MAX_USER_CHOICE = 8; // maximum user dashboard choice

std::optional<int> tryReadMenuChoice(int min, int max) {
  std::cout << "Enter your choice: ";
  std::string line;
  if (!std::getline(std::cin, line)) {
    std::cin.clear();
    return max;
  }

  try {
    const int answer = std::stoi(line);
    if (answer >= min && answer <= max) {
      std::cout << std::endl;
      return answer;
    }
  } catch (const std::exception &) {
    // fall through
  }
  return std::nullopt;
}

// prompt for a non-empty line; nullopt on "exit" or EOF
std::optional<std::string> readLine(std::string_view prompt) {
  while (true) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
      std::cin.clear();
      return std::nullopt;
    }
    std::cout << std::endl;

    if (line == "exit") {
      return std::nullopt;
    }
    if (line.empty()) {
      std::cout << ">> Value cannot be empty.\n";
      continue;
    }
    return line;
  }
}

} // namespace

// showing the home screen
int ConsoleUI::showHomeScreen() {
  while (true) {
    std::cout << "=== Distributed Chat Application ===\n";
    std::cout << "Welcome to the distributed chat application.\n";
    std::cout << "Please enter an action:\n";
    std::cout << "1. Login\n";
    std::cout << "2. Register\n";
    std::cout << "3. Exit\n";
    std::cout << "--------------------------------\n";

    if (const auto answer = tryReadMenuChoice(MIN_MENU_CHOICE, MAX_MENU_CHOICE)) {
      return *answer;
    }
    std::cout << "\nInvalid choice. Please enter a valid choice.\n\n";
  }
}

// showing the user dashboard
int ConsoleUI::showUserDashboard(const ClientState &state) {
  if (!state.user) {
    return -1;
  }

  while (true) {
    std::cout << "=== User Dashboard ===\n";
    std::cout << "Welcome " << state.user->getUsername() << ".\n";
    if (state.currentRoom) {
      std::cout << "Current room: " << state.currentRoom->getName() << ".\n";
    }
    std::cout << "1. Update Profile\n";
    std::cout << "2. Join Room\n";
    std::cout << "3. Leave Room\n";
    std::cout << "4. Send Message\n";
    std::cout << "5. Create Room\n";
    std::cout << "6. Load Message History\n";
    std::cout << "7. Invite to Room\n";
    std::cout << "8. Logout\n";
    std::cout << "--------------------------------\n";

    if (const auto answer = tryReadMenuChoice(MIN_USER_CHOICE, MAX_USER_CHOICE)) {
      return *answer;
    }
    std::cout << "\nInvalid choice. Please enter a valid choice.\n\n";
  }
}

// showing the login screen
std::optional<Packet> ConsoleUI::showLogin() {
  std::cout << ">> Please login:\n";

  const auto username = readLine(">> Username (type 'exit' to go back): ");
  if (!username) {
    return std::nullopt;
  }

  const auto password = readLine(">> Password (type 'exit' to go back): ");
  if (!password) {
    return std::nullopt;
  }

  try {
    return PacketBuilder::buildLogin(*username, *password);
  } catch (const std::invalid_argument &e) {
    std::cout << ">> " << e.what() << '\n';
    return std::nullopt;
  }
}

// showing the register screen
std::optional<Packet> ConsoleUI::showRegister() {
  std::cout << ">> Please register:\n";

  const auto username = readLine(">> New username (type 'exit' to go back): ");
  if (!username) {
    return std::nullopt;
  }

  const auto password = readLine(">> New password (type 'exit' to go back): ");
  if (!password) {
    return std::nullopt;
  }

  const auto email = readLine(">> New email (type 'exit' to go back): ");
  if (!email) {
    return std::nullopt;
  }

  try {
    return PacketBuilder::buildRegister(*username, *password, *email);
  } catch (const std::invalid_argument &e) {
    std::cout << ">> " << e.what() << '\n';
    return std::nullopt;
  }
}

// showing the join room screen
std::optional<Packet> ConsoleUI::showJoinRoom(const ClientState &state) {
  if (!state.user) {
    return std::nullopt;
  }

  const auto rooms = state.getRooms();
  std::cout << ">> Available rooms:\n";
  if (rooms.empty()) {
    std::cout << "   (none cached — type a room name anyway)\n";
  } else {
    for (const Room &room : rooms) {
      std::cout << "   - " << room.getName() << " (" << Room::roomTypeToString(room.getType())
                << ", " << Room::privacyToString(room.getPrivacy()) << ")\n";
    }
  }

  const auto roomName = readLine(">> Which room do you want to join? (type 'exit' to go back): ");
  if (!roomName) {
    return std::nullopt;
  }

  try {
    return PacketBuilder::buildJoinRoom(state.user->getUsername(), *roomName);
  } catch (const std::invalid_argument &e) {
    std::cout << ">> " << e.what() << '\n';
    return std::nullopt;
  }
}

// showing the create room screen
std::optional<Packet> ConsoleUI::showCreateRoom(const User &user) {
  std::cout << ">> Create room:\n";
  std::cout << "1. Public Room\n";
  std::cout << "2. Private Room\n";
  std::cout << "3. Back\n";
  std::cout << "--------------------------------\n";

  const auto answer = tryReadMenuChoice(1, 3);
  if (!answer || *answer == 3) {
    return std::nullopt;
  }

  const auto roomPrivacy = *answer == 1 ? Room::privacyToString(Room::Privacy::PUBLIC)
                                        : Room::privacyToString(Room::Privacy::PRIVATE);

  const auto roomName = readLine(">> New room name (type 'exit' to go back): ");
  if (!roomName) {
    return std::nullopt;
  }

  try {
    return PacketBuilder::buildCreateRoom(user.getUsername(), *roomName, roomPrivacy);
  } catch (const std::invalid_argument &e) {
    std::cout << ">> " << e.what() << '\n';
    return std::nullopt;
  }
}

// showing the invite-to-room screen
std::optional<Packet> ConsoleUI::showInviteToRoom(const ClientState &state) {
  if (!state.user) {
    return std::nullopt;
  }

  std::string room;
  if (state.currentRoom && !state.currentRoom->getName().empty()) {
    room = state.currentRoom->getName();
    std::cout << ">> Inviting into current room: " << room << '\n';
  } else {
    const auto roomName = readLine(">> Room to invite into (type 'exit' to go back): ");
    if (!roomName) {
      return std::nullopt;
    }
    room = *roomName;
  }

  const auto invitee = readLine(">> Username to invite (type 'exit' to go back): ");
  if (!invitee) {
    return std::nullopt;
  }

  try {
    return PacketBuilder::buildInviteToRoom(state.user->getUsername(), room, *invitee);
  } catch (const std::invalid_argument &e) {
    std::cout << ">> " << e.what() << '\n';
    return std::nullopt;
  }
}

// showing the create message screen
std::optional<Packet> ConsoleUI::showCreateMessage(const User &user) {
  const auto message = readLine(">> What do you want to say? (type 'exit' to go back): ");
  if (!message) {
    return std::nullopt;
  }

  try {
    return PacketBuilder::buildMessage(user.getUsername(), *message);
  } catch (const std::invalid_argument &e) {
    std::cout << ">> " << e.what() << '\n';
    return std::nullopt;
  }
}

// showing the update profile screen
std::optional<Packet> ConsoleUI::showUpdateProfile(const User &user) {
  std::cout << ">> Update profile for " << user.getUsername() << " (" << user.getEmail() << ")\n";
  std::cout << "1. Update username\n";
  std::cout << "2. Update password\n";
  std::cout << "3. Back\n";
  std::cout << "--------------------------------\n";

  const auto answer = tryReadMenuChoice(1, 3);
  if (!answer || *answer == 3) {
    return std::nullopt;
  }

  switch (*answer) {
  case 1: {
    const auto newUsername = readLine(">> New username (type 'exit' to go back): ");
    if (!newUsername) {
      return std::nullopt;
    }
    try {
      // empty password => server keeps the current password
      return PacketBuilder::buildUpdateUser(*newUsername, "", user.getEmail());
    } catch (const std::invalid_argument &e) {
      std::cout << ">> " << e.what() << '\n';
      return std::nullopt;
    }
  }
  case 2: {
    const auto newPassword = readLine(">> New password (type 'exit' to go back): ");
    if (!newPassword) {
      return std::nullopt;
    }
    try {
      return PacketBuilder::buildUpdateUser(user.getUsername(), *newPassword, user.getEmail());
    } catch (const std::invalid_argument &e) {
      std::cout << ">> " << e.what() << '\n';
      return std::nullopt;
    }
  }
  default:
    return std::nullopt;
  }
}
