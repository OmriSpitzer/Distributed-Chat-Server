/**
 * ConsoleUI class
 *
 * @brief Simple stdout-based user interface.
 * @date 14-07-2026
 */

#include "client/console_ui.h"
#include <cstdlib>
#include <iostream>

int ConsoleUI::showWelcome() {
  std::string answer = "-1";
  do {
    std::cout << "=== Distributed Chat Application ===\n";
    std::cout << "Welcome to the distributed chat application.\n";
    std::cout << "Please enter an action:\n";
    std::cout << "1. Login\n";
    std::cout << "2. Register\n";
    std::cout << "3. Exit\n";
    std::cout << "--------------------------------\n";
    std::cout << "Enter your choice: ";
    std::cin >> answer;

    if (atoi(answer) < 1 || atoi(answer) > 3) {
      std::cout << "\nInvalid choice. Please enter a valid choice.\n\n";
    }
  } while (atoi(answer) < 1 || atoi(answer) > 3);
  return atoi(answer);
}

void ConsoleUI::showLogin() { std::cout << "Please log in.\n"; }

void ConsoleUI::showRooms() { std::cout << "Available rooms:\n"; }

void ConsoleUI::printMessage() { std::cout << "<message>\n"; }

void ConsoleUI::clearScreen() {
#ifdef _WIN32
  std::system("cls");
#else
  std::system("clear");
#endif
}
