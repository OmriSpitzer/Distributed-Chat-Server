/**
 * ConsoleUI class
 *
 * @brief Simple stdout-based user interface.
 * @date 06-09-2026
 */

#include "client/console_ui.h"
#include "auth/authentication.h"
#include "client/packet_builder.h"
#include "utils/models/packet.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <string>

// showing the welcome screen
int ConsoleUI::showWelcome() {
  int answer = -1;
  std::string input = "-1";

  // show the welcome screen until the user enters a valid choice
  do {
    std::cout << "=== Distributed Chat Application ===\n";
    std::cout << "Welcome to the distributed chat application.\n";
    std::cout << "Please enter an action:\n";
    std::cout << "1. Login\n";
    std::cout << "2. Register\n";
    std::cout << "3. Exit\n";
    std::cout << "--------------------------------\n";
    std::cout << "Enter your choice: ";
    std::cin >> input;
    std::cout << std::endl;

    answer = std::stoi(input);
    if (answer < 1 || answer > 3) {
      std::cout << "\nInvalid choice. Please enter a valid choice.\n\n";
    }
  } while (answer < 1 || answer > 3);
  return answer;
}

// showing the login screen
std::optional<Packet> ConsoleUI::showLogin() {
  std::string username;
  std::string password;

  // drop the leftover '\n' from the previous cin >> in showWelcome
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  do {
    std::cout << ">> Please login:\n";
    std::cout << ">> Username (insert 'exit' to go back): ";
    std::getline(std::cin, username);
    std::cout << std::endl;

    if (username == "exit") {
      return std::nullopt;
    }
    if (!Authentication::verifyUsername(username)) {
      continue;
    }

    std::cout << ">> Password (insert 'exit' to go back): ";
    std::getline(std::cin, password);
    std::cout << std::endl;

    if (password == "exit") {
      return std::nullopt;
    }
    if (!Authentication::verifyPassword(password)) {
      continue;
    }
    break;
  } while (true);

  return PacketBuilder::buildLogin(username, password);
}

// showing the register screen
std::optional<Packet> ConsoleUI::showRegister() {
  std::string username;
  std::string password;
  std::string email;

  // drop the leftover '\n' from the previous cin >> in showWelcome
  std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

  do {
    std::cout << ">> Please register:\n";
    std::cout << ">> New username (insert 'exit' to go back): ";
    std::getline(std::cin, username);
    std::cout << std::endl;

    if (username == "exit") {
      return std::nullopt;
    }
    if (!Authentication::verifyUsername(username)) {
      continue;
    }

    std::cout << ">> New password (insert 'exit' to go back): ";
    std::getline(std::cin, password);
    std::cout << std::endl;

    if (password == "exit") {
      return std::nullopt;
    }
    if (!Authentication::verifyPassword(password)) {
      continue;
    }

    std::cout << ">> New email (insert 'exit' to go back): ";
    std::getline(std::cin, email);
    std::cout << std::endl;

    if (email == "exit") {
      return std::nullopt;
    }
    if (!Authentication::verifyEmail(email)) {
      continue;
    }

    break;
  } while (true);

  return PacketBuilder::buildRegister(username, password, email);
}
