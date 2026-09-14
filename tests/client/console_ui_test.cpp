/**
 * ConsoleUI unit tests
 *
 * @brief Includes: home / dashboard menu choices, EOF defaults, cancel via exit,
 * empty-line retry, login / register / join / message packet building.
 * @date 13-09-2026
 */

#include "client/client_state.h"
#include "client/console_ui.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

/**
 * 1. showHomeScreen accepts a valid choice
 * 2. showHomeScreen retries after invalid input
 * 3. showHomeScreen EOF returns Exit
 * 4. showUserDashboard without user returns -1
 * 5. showUserDashboard accepts a valid choice
 * 6. showUserDashboard EOF returns Logout
 * 7. showLogin builds a LOGIN packet
 * 8. showLogin cancel on username exit
 * 9. showLogin cancel on password exit
 * 10. showLogin retries empty then succeeds
 * 11. showRegister builds a REGISTER packet
 * 12. showJoinRoom builds a ROOM_JOIN packet
 * 13. showCreateMessage builds a MESSAGE packet
 * 14. showCreateMessage cancel via exit
 */

namespace {

struct IoRedirect {
  std::istringstream in;
  std::ostringstream out;
  std::streambuf *oldIn;
  std::streambuf *oldOut;

  explicit IoRedirect(const std::string &input)
      : in(input), oldIn(std::cin.rdbuf(in.rdbuf())), oldOut(std::cout.rdbuf(out.rdbuf())) {
    std::cin.clear();
  }

  ~IoRedirect() {
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
  }

  IoRedirect(const IoRedirect &) = delete;
  IoRedirect &operator=(const IoRedirect &) = delete;
};

ClientState loggedInState() {
  ClientState state;
  state.user = User("alice", "alice@example.com", User::UserType::USER);
  state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
  return state;
}

} // namespace

// 1. showHomeScreen accepts a valid choice
TEST_CASE("ConsoleUI showHomeScreen accepts a valid choice", "[console_ui][menu]") {
  IoRedirect io("2\n");
  REQUIRE(ConsoleUI::showHomeScreen() == 2);
}

// 2. showHomeScreen retries after invalid input
TEST_CASE("ConsoleUI showHomeScreen retries after invalid input", "[console_ui][menu][edge]") {
  IoRedirect io("0\nx\n1\n");
  REQUIRE(ConsoleUI::showHomeScreen() == 1);
}

// 3. showHomeScreen EOF returns Exit
TEST_CASE("ConsoleUI showHomeScreen EOF returns Exit", "[console_ui][menu][edge]") {
  IoRedirect io("");
  REQUIRE(ConsoleUI::showHomeScreen() == 3);
}

// 4. showUserDashboard without user returns -1
TEST_CASE("ConsoleUI showUserDashboard without user returns -1", "[console_ui][menu][edge]") {
  const ClientState state;
  REQUIRE(ConsoleUI::showUserDashboard(state) == -1);
}

// 5. showUserDashboard accepts a valid choice
TEST_CASE("ConsoleUI showUserDashboard accepts a valid choice", "[console_ui][menu]") {
  IoRedirect io("4\n");
  REQUIRE(ConsoleUI::showUserDashboard(loggedInState()) == 4);
}

// 6. showUserDashboard EOF returns Logout
TEST_CASE("ConsoleUI showUserDashboard EOF returns Logout", "[console_ui][menu][edge]") {
  IoRedirect io("");
  REQUIRE(ConsoleUI::showUserDashboard(loggedInState()) == 5);
}

// 7. showLogin builds a LOGIN packet
TEST_CASE("ConsoleUI showLogin builds a LOGIN packet", "[console_ui][login]") {
  IoRedirect io("alice\nsecret\n");
  const auto packet = ConsoleUI::showLogin();

  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::LOGIN);
  REQUIRE(packet->sender == "alice");
  REQUIRE(packet->message == "secret");
}

// 8. showLogin cancel on username exit
TEST_CASE("ConsoleUI showLogin cancel on username exit", "[console_ui][login][edge]") {
  IoRedirect io("exit\n");
  REQUIRE_FALSE(ConsoleUI::showLogin().has_value());
}

// 9. showLogin cancel on password exit
TEST_CASE("ConsoleUI showLogin cancel on password exit", "[console_ui][login][edge]") {
  IoRedirect io("alice\nexit\n");
  REQUIRE_FALSE(ConsoleUI::showLogin().has_value());
}

// 10. showLogin retries empty then succeeds
TEST_CASE("ConsoleUI showLogin retries empty then succeeds", "[console_ui][login][edge]") {
  IoRedirect io("\nalice\n\nsecret\n");
  const auto packet = ConsoleUI::showLogin();

  REQUIRE(packet.has_value());
  REQUIRE(packet->sender == "alice");
  REQUIRE(packet->message == "secret");
}

// 11. showRegister builds a REGISTER packet
TEST_CASE("ConsoleUI showRegister builds a REGISTER packet", "[console_ui][register]") {
  IoRedirect io("bob\npass\nbob@example.com\n");
  const auto packet = ConsoleUI::showRegister();

  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::REGISTER);
  REQUIRE(packet->sender == "bob");
  REQUIRE(packet->message == "pass");
  REQUIRE(packet->room == "bob@example.com");
}

// 12. showJoinRoom builds a ROOM_JOIN packet
TEST_CASE("ConsoleUI showJoinRoom builds a ROOM_JOIN packet", "[console_ui][join]") {
  const User user("alice", "alice@example.com", User::UserType::USER);
  IoRedirect io("General\n");
  const auto packet = ConsoleUI::showJoinRoom(user);

  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::ROOM_JOIN);
  REQUIRE(packet->sender == "alice");
  REQUIRE(packet->room == "General");
}

// 13. showCreateMessage builds a MESSAGE packet
TEST_CASE("ConsoleUI showCreateMessage builds a MESSAGE packet", "[console_ui][message]") {
  const User user("alice", "alice@example.com", User::UserType::USER);
  IoRedirect io("hello world\n");
  const auto packet = ConsoleUI::showCreateMessage(user);

  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::MESSAGE);
  REQUIRE(packet->sender == "alice");
  REQUIRE(packet->message == "hello world");
}

// 14. showCreateMessage cancel via exit
TEST_CASE("ConsoleUI showCreateMessage cancel via exit", "[console_ui][message][edge]") {
  const User user("alice", "alice@example.com", User::UserType::USER);
  IoRedirect io("exit\n");
  REQUIRE_FALSE(ConsoleUI::showCreateMessage(user).has_value());
}

// 15. showUpdateProfile username change
TEST_CASE("ConsoleUI showUpdateProfile builds username UPDATE_USER", "[console_ui][update]") {
  const User user("alice", "alice@example.com", User::UserType::USER);
  IoRedirect io("1\nbob\n");
  const auto packet = ConsoleUI::showUpdateProfile(user);

  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::UPDATE_USER);
  REQUIRE(packet->sender == "bob");
  REQUIRE(packet->message.empty());
  REQUIRE(packet->room == "alice@example.com");
}

// 16. showUpdateProfile password change
TEST_CASE("ConsoleUI showUpdateProfile builds password UPDATE_USER", "[console_ui][update]") {
  const User user("alice", "alice@example.com", User::UserType::USER);
  IoRedirect io("2\nnewsecret\n");
  const auto packet = ConsoleUI::showUpdateProfile(user);

  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::UPDATE_USER);
  REQUIRE(packet->sender == "alice");
  REQUIRE(packet->message == "newsecret");
  REQUIRE(packet->room == "alice@example.com");
}

// 17. showUpdateProfile back cancels
TEST_CASE("ConsoleUI showUpdateProfile back cancels", "[console_ui][update][edge]") {
  const User user("alice", "alice@example.com", User::UserType::USER);
  IoRedirect io("3\n");
  REQUIRE_FALSE(ConsoleUI::showUpdateProfile(user).has_value());
}
