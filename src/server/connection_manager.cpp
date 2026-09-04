/**
 * ConnectionManager class
 *
 * @brief Owns the listening socket and client sessions.
 * @date 14-07-2026
 */

#include "server/connection_manager.h"
#include "utils/models/logger.h"
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

// constructor
ConnectionManager::ConnectionManager() : listeningSocket(-1), listening(false) {}

// destructor
ConnectionManager::~ConnectionManager() { stopListening(); }

// start listening on given port
bool ConnectionManager::startListening(std::uint16_t port) {
  // check if already listening
  if (listening) {
    Logger::logWarning("ConnectionManager", "Already listening");
    return false;
  }

  // create socket and check if successful
  SOCKET socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketFd == INVALID_SOCKET) {
    Logger::logError("ConnectionManager", "Failed to create socket");
    return false;
  }

  // allow quick rebinds after restart
  int reuse = 1;
  if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse),
                 sizeof(reuse)) != 0) {
    Logger::logWarning("ConnectionManager", "setsockopt(SO_REUSEADDR) failed");
  }

  // bind socket to port
  sockaddr_in address{};
  address.sin_family = AF_INET;                // set address family
  address.sin_addr.s_addr = htonl(INADDR_ANY); // bind to all interfaces
  address.sin_port = htons(port);              // bind to port

  // bind socket to port and check if successful
  if (bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    Logger::logError("ConnectionManager", "Failed to bind port " + std::to_string(port));
    closesocket(socketFd);
    return false;
  }

  // listen for connections and check if successful
  if (listen(socketFd, SOMAXCONN) != 0) {
    Logger::logError("ConnectionManager", "Failed to listen on port " + std::to_string(port));
    closesocket(socketFd);
    return false;
  }

  // set listening socket and mark as listening
  listeningSocket = static_cast<int>(socketFd);
  listening = true;
  Logger::logInfo("ConnectionManager", "Listening on port " + std::to_string(port));
  return true;
}

// stop listening
void ConnectionManager::stopListening() {
  // check if listening
  if (!listening) {
    Logger::logWarning("ConnectionManager", "Not listening");
    return;
  }

  // close socket
  closesocket(static_cast<SOCKET>(listeningSocket));
  listeningSocket = -1;
  listening = false;
}

// get listening socket
int ConnectionManager::getListeningSocket() const { return listeningSocket; }

// check if listening
bool ConnectionManager::isListening() const { return listening; }
