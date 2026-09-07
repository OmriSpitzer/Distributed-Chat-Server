/**
 * ConnectionManager class
 *
 * @brief Owns the listening socket and client sessions.
 * @date 06-09-2026
 */

#include "server/connection_manager.h"
#include "server/packet_processor.h"
#include "server/room_manager.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/serializer.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

namespace {
constexpr std::uint32_t kMaxPayloadBytes = 1024 * 1024; // same as Serializer

bool recvExact(SOCKET socket, char *buffer, int bytes) {
  int received = 0;
  while (received < bytes) {
    int n = recv(socket, buffer + received, bytes - received, 0);
    if (n <= 0) {
      return false; // disconnect or error
    }
    received += n;
  }
  return true;
}

bool sendExact(SOCKET socket, const char *buffer, int bytes) {
  int sent = 0;
  while (sent < bytes) {
    int n = send(socket, buffer + sent, bytes - sent, 0);
    if (n == SOCKET_ERROR) {
      return false;
    }
    sent += n;
  }
  return true;
}

std::optional<Packet> readPacket(SOCKET socket) {
  char sizeBuf[4];
  if (!recvExact(socket, sizeBuf, 4)) {
    return std::nullopt;
  }

  std::uint32_t payloadSize =
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[0])) << 24) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[1])) << 16) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[2])) << 8) |
      static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[3]));

  if (payloadSize == 0 || payloadSize > kMaxPayloadBytes) {
    return std::nullopt;
  }

  std::string framed(4 + payloadSize, '\0');
  framed[0] = sizeBuf[0];
  framed[1] = sizeBuf[1];
  framed[2] = sizeBuf[2];
  framed[3] = sizeBuf[3];

  if (!recvExact(socket, framed.data() + 4, static_cast<int>(payloadSize))) {
    return std::nullopt;
  }

  return Serializer::deserialize(framed);
}
} // namespace

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

// get sessions
std::unordered_map<int, std::shared_ptr<ClientSession>> ConnectionManager::getSessions() const {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  return sessions;
}

// accept loop
void ConnectionManager::acceptLoop() {
  while (listening) {
    SOCKET client = accept(static_cast<SOCKET>(listeningSocket), nullptr, nullptr);
    if (client == INVALID_SOCKET) {
      break; // stopListening() closed the socket
    }

    int fd = static_cast<int>(client);
    Logger::logInfo("ConnectionManager", "New client connected socket " + std::to_string(fd));

    addSession(fd, std::make_shared<ClientSession>(fd, User::anonymousUser(), RoomManager::LOBBY));

    // one blocking read-loop per client
    std::thread([this, fd] { handleClient(fd); }).detach();
  }
}

// handle the client
void ConnectionManager::handleClient(int clientSocket) {
  SOCKET sock = static_cast<SOCKET>(clientSocket);

  while (listening) {
    std::optional<Packet> packet = readPacket(sock);
    if (!packet) {
      break;
    }

    std::shared_ptr<ClientSession> session;
    {
      std::lock_guard<std::mutex> lock(sessionsMutex);
      auto it = sessions.find(clientSocket);
      if (it == sessions.end()) {
        break;
      }
      session = it->second;
    }

    // process the packet
    Packet response = PacketProcessor::processPacket(*packet, *session, *this);

    // generate a response packet
    std::string framed = Serializer::serialize(response);
    if (framed.empty() || !sendExact(sock, framed.data(), static_cast<int>(framed.size()))) {
      break;
    }
  }

  closesocket(sock);
  removeSession(clientSocket);
  Logger::logInfo("ConnectionManager",
                  "Client disconnected, socket " + std::to_string(clientSocket));
}

// add new session
void ConnectionManager::addSession(int socket, std::shared_ptr<ClientSession> session) {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  sessions[socket] = std::move(session);
}

// remove session
void ConnectionManager::removeSession(int socket) {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  sessions.erase(socket);
}

// does the user have a session
bool ConnectionManager::hasSession(const User &user) const {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  for (const auto &entry : sessions) {
    const ClientSession &session = *entry.second;
    if (session.getUser() == user) {
      return true;
    }
  }
  return false;
}