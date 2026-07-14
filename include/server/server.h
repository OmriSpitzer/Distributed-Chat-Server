/**
 * Server header file class
 *
 * @date 14-07-2026
 */

#pragma once
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/packet_processor.h"
#include "server/thread_pool.h"

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
  ThreadPool threadPool{config::THREAD_COUNT}; // worker threads
  ConnectionManager connectionManager;         // client connections
  PacketProcessor processor;                   // request routing
  bool running = false;                        // running flag
};