/**
 * Client header file class
 *
 * @date 14-07-2026
 */

#pragma once
#include "client/console_ui.h"
#include "client/network.h"
#include "client/packet_builder.h"
#include "client/packet_handler.h"


class Client {
public:
  // starting the client
  void start();

  // stopping the client
  void stop();

private:
  Network network; // network class
  ConsoleUI ui;    // console UI class
  // CommandParser parser;  // command parser class
  PacketHandler handler; // packet handler class
  PacketBuilder builder; // packet builder class
};