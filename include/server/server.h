/**
 * Server class header file
 *
 * @date 11-09-2026
 */

#pragma once
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/gossip_manager.h"
#include "server/heartbeat.h"
#include "server/thread_pool.h"
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
  ThreadPool threadPool{config::THREAD_COUNT};                    // worker threads
  ConnectionManager connectionManager{};                          // client connections
  GossipManager gossipManager = GossipManager(connectionManager); // server gossip manager
  Heartbeat heartbeat = Heartbeat(connectionManager);             // heartbeat
  HealthMonitor healthMonitor = HealthMonitor();                  // health monitor

  std::thread acceptThread;         // accept thread
  std::atomic<bool> running{false}; // running flag
};