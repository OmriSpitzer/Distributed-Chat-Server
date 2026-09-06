/**
 * Server class
 *
 * @brief Wires together the server components and owns the listen lifecycle.
 * @date 03-09-2026
 */

#include "server/server.h"
#include "config/config.h"
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

  running = true;
  heartbeat.start();
  Logger::logInfo("Server", "Started on port " + std::to_string(config::PORT) + " with " +
                                std::to_string(config::THREAD_COUNT) + " worker threads");
}

// stop the server
void Server::stop() {
  // check if the server is not running
  if (!running) {
    Logger::logInfo("Server", "Server is not running");
    return;
  }

  running = false;

  // stop listening and shutdown thread pool
  heartbeat.stop();
  connectionManager.stopListening();
  threadPool.shutdown();

  WSACleanup();
  Logger::logInfo("Server", "Server stopped");
}

// dashboard of the server
int Server::dashboard() {
  std::cout << "--------------------------------" << std::endl;
  std::cout << "Server dashboard" << std::endl;
  std::cout << "--------------------------------" << std::endl;
  std::cout << "Port: " << config::PORT << " Thread count: " << config::THREAD_COUNT << std::endl;
  std::cout << "Database path: " << config::DB_PATH << std::endl;
  std::cout << "Listening: " << (connectionManager.isListening() ? "yes" : "no") << std::endl;
  std::cout << "--------------------------------\n" << std::endl;
  return 0;
}

// check if the server is alive
bool Server::isAlive() { return running; }
