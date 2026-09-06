/**
 * Client header file class
 *
 * @date 04-09-2026
 */

#pragma once
#include "client/console_ui.h"
#include "client/network.h"
#include "client/packet_builder.h"
#include "client/packet_handler.h"
#include <string>

class Client {
public:
  // starting the client
  bool start();

  // stopping the client
  void stop();

  // check if the client is alive
  bool isAlive() const;

  // showing the dashboard
  void showDashboard();

private:
  std::string id;        // client id
  Network network;       // network class
  ConsoleUI ui;          // console UI class
  PacketHandler handler; // packet handler class
  PacketBuilder builder; // packet builder class
};