/**
 * Server class
 *
 * @brief Wires together the server components and owns the listen lifecycle.
 * @date 11-09-2026
 */

#include "server/server.h"
#include "config/config.h"
#include "server/gossip_manager.h"
#include "server/heartbeat.h"
#include "utils/models/logger.h"
#include <iostream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

// start the server
void Server::start() {
  Logger::logInfo("Server", "Starting server");

  // check if the server is already running
  if (running) {
    Logger::logInfo("Server", "Server is already running");
    return;
  }

  // initialize winsock 2.2
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    Logger::logError("Server", "WSAStartup failed");
    return;
  }

  // start listening on the port
  if (!connectionManager.startListening(config::PORT)) {
    Logger::logError("Server", "Failed to start listening on port " + std::to_string(config::PORT));
    WSACleanup();
    return;
  }

  running = true; // set the server to running

  // start heartbeat thread
  heartbeat.start();

  // create gossip manager thread
  connectionManager.setGossip(&gossipManager);
  gossipManager.start();

  // start the accept loop
  acceptThread = std::thread([this] { connectionManager.acceptLoop(); }); // start the accept loop

  Logger::logInfo("Server", "Started on port " + std::to_string(config::PORT) + " with " +
                                std::to_string(config::THREAD_COUNT) +
                                " worker threads"); // log the server started
}

// stop the server
void Server::stop() {
  // check if the server is not running
  if (!running) {
    Logger::logInfo("Server", "Server is not running");
    return;
  }

  // set the server to not running
  running = false;

  // stop all threads
  heartbeat.stop();
  gossipManager.stop();
  connectionManager.setGossip(nullptr);
  connectionManager.stopListening();
  if (acceptThread.joinable()) {
    acceptThread.join();
  }
  threadPool.shutdown();

  // clean up winsock
  WSACleanup();
  Logger::logInfo("Server", "Server stopped");
}

// dashboard of the server
void Server::dashboard() {
  std::cout << "--------------------------------" << std::endl;
  std::cout << "Server dashboard" << std::endl;
  std::cout << "--------------------------------" << std::endl;
  std::cout << "Node id: " << config::NODE_ID << std::endl;
  std::cout << "Port: " << config::PORT << " Peer port: " << config::PEER_PORT
            << " Thread count: " << config::THREAD_COUNT << std::endl;
  std::cout << "Database path: " << config::DB_PATH << std::endl;
  std::cout << "Peers: ";
  if (config::PEERS.empty()) {
    std::cout << "(none)";
  } else {
    for (std::size_t i = 0; i < config::PEERS.size(); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << config::PEERS[i];
    }
  }
  std::cout << std::endl;
  std::cout << "Listening: " << (connectionManager.isListening() ? "yes" : "no") << std::endl;
  std::cout << "--------------------------------\n" << std::endl;
}

// check if the server is alive
bool Server::isAlive() { return running; }

// destructor
Server::~Server() { stop(); }