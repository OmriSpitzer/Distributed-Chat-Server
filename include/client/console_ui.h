/**
 * ConsoleUI header file class
 *
 * @date 04-09-2026
 */
#pragma once

class ConsoleUI {
public:
  // showing the welcome screen
  static int showWelcome();

  // showing the login screen
  static void showLogin();

  // showing the rooms screen
  static void showRooms();

  // printing a message
  static void printMessage();

  // showing an error
  static void showError();

  // clearing the screen
  static void clearScreen();
};
