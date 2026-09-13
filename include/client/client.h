/**
 * Client header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "client/client_state.h"
#include "client/network.h"
#include "client/packet_handler.h"
#include "utils/models/packet.h"
#include <optional>
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
  PacketHandler handler; // packet handler class
  ClientState state;     // client state class

  // waiting for a packet of a specific type
  std::optional<Packet> waitFor(Packet::PacketType expected);
};