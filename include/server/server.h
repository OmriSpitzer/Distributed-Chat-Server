/**
 * Server header file class
 *
 * @date 10-09-2026
 */

#pragma once
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/gossip_manager.h"
#include "server/heartbeat.h"
#include "server/packet_processor.h"
#include "server/thread_pool.h"
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
  int dashboard();

private:
  ThreadPool threadPool{config::THREAD_COUNT};        // worker threads
  ConnectionManager connectionManager;                // client connections
  PacketProcessor processor;                          // request routing
  bool running = false;                               // running flag
  Heartbeat heartbeat = Heartbeat(connectionManager);             // heartbeat
  GossipManager gossipManager = GossipManager(connectionManager); // gossip mesh
  std::thread acceptThread;                                       // accept thread
};