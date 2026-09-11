/**
 * ConnectionManager class
 *
 * @brief Owns the listening socket and client sessions.
 * @date 11-09-2026
 */

#include "server/connection_manager.h"
#include "config/config.h"
#include "server/database_manager.h"
#include "server/gossip_manager.h"
#include "server/packet_processor.h"
#include "server/room_manager.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/serializer.h"
#include <atomic>
#include <cstdint>
#include <ctime>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

namespace {
// receive exact number of bytes
bool recvExact(SOCKET socket, char *buffer, int bytes) {
  int received = 0;
  while (received < bytes) {
    // number of bytes received
    int n = recv(socket, buffer + received, bytes - received, 0);

    if (n <= 0) {
      return false;
    }
    received += n;
  }
  return true;
}

// send exact number of bytes
bool sendExact(SOCKET socket, const char *buffer, int bytes) {
  int sent = 0;
  while (sent < bytes) {

    // number of bytes sent
    int n = send(socket, buffer + sent, bytes - sent, 0);
    if (n <= 0) {
      return false;
    }
    sent += n;
  }
  return true;
}

// read a packet from the socket
std::optional<Packet> readPacket(SOCKET socket) {
  char sizeBuf[4];

  // check if the size buffer is received
  if (!recvExact(socket, sizeBuf, 4)) {
    return std::nullopt;
  }

  // convert the size buffer to a 32-bit unsigned integer
  std::uint32_t payloadSize =
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[0])) << 24) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[1])) << 16) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[2])) << 8) |
      static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[3]));

  // check if the payload size is valid
  if (payloadSize == 0 || payloadSize > Serializer::MAX_PAYLOAD_BYTES) {
    return std::nullopt;
  }

  // create the framed buffer
  std::string framed(4 + payloadSize, '\0');
  framed[0] = sizeBuf[0];
  framed[1] = sizeBuf[1];
  framed[2] = sizeBuf[2];
  framed[3] = sizeBuf[3];

  // check if the payload is received
  if (!recvExact(socket, framed.data() + 4, static_cast<int>(payloadSize))) {
    return std::nullopt;
  }

  // deserialize the packet
  return Serializer::deserialize(framed);
}
} // namespace

/* --------------------------------------------------
 * ConnectionManager class implementation
 * --------------------------------------------------
 */

// constructor
ConnectionManager::ConnectionManager() : listeningSocket(-1) {}

// destructor
ConnectionManager::~ConnectionManager() { stopListening(); }

// set the gossip manager
void ConnectionManager::setGossip(GossipManager *gossip) { gossip_ = gossip; }

// create a rumor for the gossip manager
void ConnectionManager::rumor(const Packet &event) {
  if (gossip_) {
    gossip_->rumor(event);
  }
}

// start listening on given port
bool ConnectionManager::startListening(std::uint16_t port) {
  // check if already listening
  if (listening.load()) {
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
  listening.store(true);
  Logger::logInfo("ConnectionManager", "Listening on port " + std::to_string(port));
  return true;
}

// stop listening
void ConnectionManager::stopListening() {
  // mark stopped; wake accept + all blocked client reads
  if (!listening.exchange(false)) {
    Logger::logWarning("ConnectionManager", "Not listening");
    return;
  }

  if (listeningSocket != -1) {
    closesocket(static_cast<SOCKET>(listeningSocket));
    listeningSocket = -1;
  }

  std::vector<int> clientSockets;
  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    clientSockets.reserve(sessions.size());
    for (const auto &entry : sessions) {
      clientSockets.push_back(entry.first);
    }
  }

  // close all client sockets
  for (int fd : clientSockets) {
    closeClient(fd);
  }
}

// get listening socket
int ConnectionManager::getListeningSocket() const { return listeningSocket; }

// check if listening
bool ConnectionManager::isListening() const { return listening.load(); }

// get sessions
std::unordered_map<int, std::shared_ptr<ClientSession>> ConnectionManager::getSessions() const {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  return sessions;
}

// accept loop
void ConnectionManager::acceptLoop() {
  while (listening.load()) {
    SOCKET client = accept(static_cast<SOCKET>(listeningSocket), nullptr, nullptr);

    // check if the client socket is valid
    if (client == INVALID_SOCKET) {
      break;
    }

    int fd = static_cast<int>(client);
    Logger::logInfo("ConnectionManager", "New client connected socket " + std::to_string(fd));

    // add the new client session (non-copyable: mutex + atomic closed flag)
    addSession(fd, std::make_shared<ClientSession>(fd, User::anonymousUser(), RoomManager::LOBBY));

    // handle the client in a new thread
    std::thread([this, fd] { handleClient(fd); }).detach();
  }
}

// handle the client
void ConnectionManager::handleClient(int clientSocket) {
  SOCKET sock = static_cast<SOCKET>(clientSocket);

  while (listening.load()) {
    // read a packet from the client socket
    std::optional<Packet> packet = readPacket(sock);

    // check if the packet is valid
    if (!packet) {
      break;
    }

    // find the session for the client socket
    std::shared_ptr<ClientSession> session;
    {
      std::lock_guard<std::mutex> lock(sessionsMutex);
      auto it = sessions.find(clientSocket);
      if (it == sessions.end()) {
        break;
      }
      session = it->second;
    }

    // touch the session to keep it alive
    session->touch();

    // ignore all heartbeats except client pings
    if (packet->type == Packet::PacketType::HEARTBEAT && packet->message != "ping") {
      continue;
    }

    // process the packet
    Packet response = PacketProcessor::processPacket(*packet, *session, *this);

    // send the response packet to the client
    if (!sendPacket(clientSocket, response)) {
      break;
    }
  }

  // close at most once (heartbeat / stopListening may have closed already)
  closeClient(clientSocket);
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
  // find the session for the client socket
  std::shared_ptr<ClientSession> session;
  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = sessions.find(socket);

    // check if the session is found
    if (it != sessions.end()) {
      session = it->second;
      sessions.erase(it);
    }
  }
  if (!session) {
    return;
  }

  const bool wasAuth = session->isAuthenticated();
  const std::string username = session->getUser().getUsername();

  // leave all rooms the user is in
  RoomManager::getInstance().leaveAll(*session);

  // if the user is not authenticated or the username is empty, return
  if (!wasAuth || username.empty()) {
    return;
  }

  // create a gossip event if the gossip manager is set
  if (gossip_) {
    // create a gossip event packet
    static std::atomic<std::uint64_t> dropLogoutSeq{0};
    const std::string eventId = config::NODE_ID + "-LOGOUT-" + username + "-" +
                                std::to_string(static_cast<long long>(std::time(nullptr))) + "-" +
                                std::to_string(++dropLogoutSeq);
    const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
    const std::string payload =
        "LOGOUT|" + eventId + "|" + username + "|" + config::NODE_ID + "|" + ts;
    Packet event(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, "", payload);

    // rumor the gossip event
    gossip_->rumor(event);

  } else {
    // clear the online status and all memberships if the gossip manager is not set
    try {
      auto &db = DatabaseManager::getInstance();
      db.clearOnline(username);
      db.clearAllMembership(username);
    } catch (...) {
    }
  }
}

// does the user have a session
bool ConnectionManager::hasSession(const User &user) const {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  for (const auto &entry : sessions) {
    const ClientSession &session = *entry.second;
    if (session.isAuthenticated() && session.getUser() == user) {
      return true;
    }
  }
  return false;
}

// send a packet to a connected client
bool ConnectionManager::sendPacket(int socket, const Packet &packet) {
  std::string framed = Serializer::serialize(packet);

  // check if the framed packet is empty
  if (framed.empty()) {
    return false;
  }

  std::shared_ptr<ClientSession> session;
  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = sessions.find(socket);
    if (it == sessions.end()) {
      return false;
    }
    session = it->second;
  }

  if (session->isClosed()) {
    return false;
  }

  // per-socket lock: heartbeats / broadcasts / replies can run in parallel across clients
  std::lock_guard<std::mutex> lock(session->sendMutex());
  if (session->isClosed()) {
    return false;
  }
  return sendExact(static_cast<SOCKET>(socket), framed.data(), static_cast<int>(framed.size()));
}

// close a client socket so its read loop exits (at most once)
void ConnectionManager::closeClient(int socket) {
  std::shared_ptr<ClientSession> session;
  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = sessions.find(socket);
    if (it == sessions.end()) {
      return;
    }
    session = it->second;
  }

  if (session->markClosed()) {
    closesocket(static_cast<SOCKET>(socket));
  }
}