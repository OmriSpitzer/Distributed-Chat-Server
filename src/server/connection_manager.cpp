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
#include "utils/gossip_payload.h"
#include "utils/logger/logger.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include "utils/socket_io.h"
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

// constructor
ConnectionManager::ConnectionManager() : listeningSocket(INVALID_SOCKET) {}

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

void ConnectionManager::pushServerDirectory(const std::string &body, SOCKET only) {
  Packet packet("server", "*", Packet::PacketType::SERVER_DIRECTORY, "", body, 0);
  if (only != INVALID_SOCKET) {
    sendPacket(only, packet);
    return;
  }

  for (const auto &entry : getSessions()) {
    if (entry.second && !entry.second->isClosed())
      sendPacket(entry.first, packet);
  }
}

void ConnectionManager::refreshServerDirectory(SOCKET only) {
  if (!gossip_)
    return;
  pushServerDirectory(gossip_->buildServerDirectory(), only);
}

// start listening on given port
bool ConnectionManager::startListening(std::uint16_t port) {
  // check if already listening
  if (listening.load()) {
    Logger::logWarning("ConnectionManager", "Already listening");
    return false;
  }

  SOCKET socketFd = socket_io::listenTo(port);
  if (socketFd == INVALID_SOCKET) {
    Logger::logError("ConnectionManager", "Failed to listen on port " + std::to_string(port));
    return false;
  }

  listeningSocket = socketFd;
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

  if (listeningSocket != INVALID_SOCKET) {
    socket_io::close(listeningSocket);
    listeningSocket = INVALID_SOCKET;
  }

  std::vector<SOCKET> clientSockets;
  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    clientSockets.reserve(sessions.size());
    for (const auto &entry : sessions) {
      clientSockets.push_back(entry.second->getSocket());
    }
  }

  // close all client sockets
  for (SOCKET fd : clientSockets) {
    closeClient(fd);
  }
}

// get listening socket
SOCKET ConnectionManager::getListeningSocket() const { return listeningSocket; }

// check if listening
bool ConnectionManager::isListening() const { return listening.load(); }

// get sessions
std::unordered_map<SOCKET, std::shared_ptr<ClientSession>> ConnectionManager::getSessions() const {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  return sessions;
}

// accept loop
void ConnectionManager::acceptLoop() {
  while (listening.load()) {
    SOCKET client = socket_io::acceptFrom(listeningSocket);

    // check if the client socket is valid
    if (client == INVALID_SOCKET) {
      break;
    }

    SOCKET fd = client;
    Logger::logInfo("ConnectionManager", "New client connected socket " + std::to_string(fd));

    // add the new client session (non-copyable: mutex + atomic closed flag)
    auto session = std::make_shared<ClientSession>(fd, User::anonymousUser(), RoomManager::LOBBY);
    addSession(fd, session);

    // register in Lobby membership so room broadcasts reach this socket
    if (!RoomManager::getInstance().joinRoom(RoomManager::LOBBY.getName(), *session)) {
      Logger::logWarning("ConnectionManager",
                         "Failed to place new client in Lobby, socket " + std::to_string(fd));
      closeClient(fd);
      removeSession(fd);
      continue;
    }

    // push connect snapshot: anon user (sender), Lobby (room), directory (message)
    std::vector<Room> directory = RoomManager::getInstance().listRooms();
    if (directory.empty()) {
      directory = {RoomManager::LOBBY, RoomManager::GENERAL};
    }
    Packet welcome("server", session->getUser().serialize(), Packet::PacketType::ROOM_LIST,
                   RoomManager::LOBBY.getName(), Room::serializeList(directory), 0);
    if (!sendPacket(fd, welcome)) {
      Logger::logWarning("ConnectionManager",
                         "Failed to send connect welcome, socket " + std::to_string(fd));
      closeClient(fd);
      removeSession(fd);
      continue;
    }

    Logger::logInfo("ConnectionManager", "Sent connect welcome with " +
                                             std::to_string(directory.size()) + " rooms, socket " +
                                             std::to_string(fd));

    // push live chat-endpoint directory (failover hints) when gossip is wired
    if (gossip_) {
      pushServerDirectory(gossip_->buildServerDirectory(), fd);
    }

    // handle the client in a new thread
    std::thread([this, fd] { handleClient(fd); }).detach();
  }
}

// handle the client
void ConnectionManager::handleClient(SOCKET clientSocket) {
  while (listening.load()) {
    // read a packet from the client socket
    std::optional<Packet> packet = socket_io::readPacket(clientSocket);

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
void ConnectionManager::addSession(SOCKET socket, std::shared_ptr<ClientSession> session) {
  std::lock_guard<std::mutex> lock(sessionsMutex);
  sessions[socket] = std::move(session);
}

// remove session
void ConnectionManager::removeSession(SOCKET socket) {
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
        gossip_payload::encode("LOGOUT", eventId, username, config::NODE_ID, ts);
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
bool ConnectionManager::sendPacket(SOCKET socket, const Packet &packet) {
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
  return socket_io::writePacket(socket, packet);
}

// close a client socket so its read loop exits (at most once)
void ConnectionManager::closeClient(SOCKET socket) {
  std::shared_ptr<ClientSession> session;
  {
    std::lock_guard<std::mutex> lock(sessionsMutex);
    auto it = sessions.find(socket);
    if (it == sessions.end()) {
      return;
    }
    session = it->second;
  }

  std::lock_guard<std::mutex> send(session->sendMutex());
  if (session->markClosed()) {
    socket_io::close(socket);
  }
}