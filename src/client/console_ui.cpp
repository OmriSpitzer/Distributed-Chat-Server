/**
 * ConsoleUI class
 *
 * @brief Simple stdout-based user interface.
 * @date 14-07-2026
 */

#include "client/console_ui.h"
#include <cstdlib>
#include <iostream>

void ConsoleUI::showLogin() {
  std::cout << "=== Distributed Chat ===\n";
  std::cout << "Please log in.\n";
}

void ConsoleUI::showRooms() { std::cout << "Available rooms:\n"; }

void ConsoleUI::printMessage() { std::cout << "<message>\n"; }

void ConsoleUI::showError() { std::cout << "An error occurred.\n"; }

void ConsoleUI::clearScreen() {
#ifdef _WIN32
  std::system("cls");
#else
  std::system("clear");
#endif
}
