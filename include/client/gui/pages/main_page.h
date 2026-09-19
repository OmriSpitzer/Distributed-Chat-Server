/**
 * Client GUI entry point
 *
 * @date 16-09-2026
 */

#pragma once

class Client;

class MainPage {
public:
  // run the client GUI (blocks until the window closes)
  static int run(int argc, char *argv[], Client &client);
};
