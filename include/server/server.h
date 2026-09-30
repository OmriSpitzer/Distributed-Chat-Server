/**
 * Server class header file
 *
 * @date 11-09-2026
 */

#pragma once
#include "server/connection_manager.h"
#include "server/gossip_manager.h"
#include "server/heartbeat.h"
#include "server/web_connection.h"
#include "utils/health/health_monitor.h"
#include <atomic>
#include <thread>

class Server {
public:
  // starting the server
  void start();

  // stopping the server
  void stop();

  // check if the server is alive
  bool isAlive();

  // dashboard of the server
  void dashboard();

  // destructor
  ~Server();

  // get connections
  ConnectionManager &connections();

  // get gossip manager
  GossipManager &gossip();

  // get health monitor
  HealthMonitor &health();

private:
  ConnectionManager connectionManager{};                          // client connections
  GossipManager gossipManager = GossipManager(connectionManager); // server gossip manager
  Heartbeat heartbeat = Heartbeat(connectionManager);             // heartbeat
  web_connection::WebConnection webConnection{connectionManager}; // browser WebSocket listen
  HealthMonitor healthMonitor = HealthMonitor();                  // health monitor

  std::thread acceptThread;         // TCP accept thread
  std::thread webAcceptThread;      // WebSocket accept thread
  std::atomic<bool> running{false}; // running flag
};