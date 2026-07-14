/**
 * Server class
 *
 * @brief Wires together the server components and owns the listen lifecycle.
 * @date 14-07-2026
 */

#include "server/server.h"
#include "config/config.h"
#include "utils/models/logger.h"
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif

namespace {

bool initSockets() {
#ifdef _WIN32
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    Logger::logError("Server", "WSAStartup failed");
    return false;
  }
#endif
  return true;
}

void cleanupSockets() {
#ifdef _WIN32
  WSACleanup();
#endif
}

} // namespace

// start the server
void Server::start() {
  // check if the server is already running
  if (running) {
    Logger::logInfo("Server", "Server is already running");
    return;
  }

  if (!initSockets()) {
    return;
  }

  if (!connectionManager.startListening(config::PORT)) {
    cleanupSockets();
    return;
  }

  running = true;
  Logger::logInfo("Server", "Starting on port " + std::to_string(config::PORT) + " with " +
                                std::to_string(config::THREAD_COUNT) + " worker threads");
  // TODO: accept connections and dispatch packets through the processor.
}

// stop the server
void Server::stop() {
  // check if the server is not running
  if (!running) {
    Logger::logInfo("Server", "Server is not running");
    return;
  }

  running = false;
  connectionManager.stopListening();
  threadPool.shutdown();
  cleanupSockets();
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
