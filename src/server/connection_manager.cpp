/**
 * ConnectionManager class
 *
 * @brief Owns the listening socket and client sessions.
 * @date 14-07-2026
 */

#include "server/connection_manager.h"
#include "utils/models/logger.h"
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
static int closesocket(int fd) { return ::close(fd); }
#endif

namespace {

SOCKET asSocket(std::uintptr_t handle) { return static_cast<SOCKET>(handle); }

std::uintptr_t fromSocket(SOCKET socket) { return static_cast<std::uintptr_t>(socket); }

} // namespace

ConnectionManager::ConnectionManager()
    : listeningSocket(fromSocket(INVALID_SOCKET)), running(false) {}

ConnectionManager::~ConnectionManager() { stopListening(); }

bool ConnectionManager::startListening(std::uint16_t port) {
  if (running) {
    Logger::logWarning("ConnectionManager", "Already listening");
    return false;
  }

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

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  if (bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    Logger::logError("ConnectionManager", "Failed to bind port " + std::to_string(port));
    closesocket(socketFd);
    return false;
  }

  if (listen(socketFd, SOMAXCONN) != 0) {
    Logger::logError("ConnectionManager", "Failed to listen on port " + std::to_string(port));
    closesocket(socketFd);
    return false;
  }

  listeningSocket = fromSocket(socketFd);
  running = true;
  Logger::logInfo("ConnectionManager", "Listening on port " + std::to_string(port));
  return true;
}

void ConnectionManager::stopListening() {
  SOCKET socketFd = asSocket(listeningSocket);
  if (socketFd != INVALID_SOCKET) {
    closesocket(socketFd);
    listeningSocket = fromSocket(INVALID_SOCKET);
  }
  running = false;
}

std::uintptr_t ConnectionManager::getListeningSocket() const { return listeningSocket; }

bool ConnectionManager::isListening() const { return running; }
