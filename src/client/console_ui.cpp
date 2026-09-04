/**
 * ConsoleUI class
 *
 * @brief Simple stdout-based user interface.
 * @date 04-09-2026
 */

#include "client/console_ui.h"
#include <cstdlib>
#include <iostream>

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

    answer = std::stoi(input);
    if (answer < 1 || answer > 3) {
      std::cout << "\nInvalid choice. Please enter a valid choice.\n\n";
    }
  } while (answer < 1 || answer > 3);
  return answer;
}

// showing the login screen
void ConsoleUI::showLogin() { std::cout << "Please log in.\n"; }

// showing the rooms screen
void ConsoleUI::showRooms() { std::cout << "Available rooms:\n"; }

// printing a message
void ConsoleUI::printMessage() { std::cout << "<message>\n"; }

// clearing the screen
void ConsoleUI::clearScreen() {
#ifdef _WIN32
  std::system("cls");
#else
  std::system("clear");
#endif
}
