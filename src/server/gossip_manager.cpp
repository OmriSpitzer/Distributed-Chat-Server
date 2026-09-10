/**
 * GossipManager class
 *
 * @date 10-09-2026
 */
#include "server/gossip_manager.h"
#include "config/config.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/models/logger.h"
#include "utils/models/message.h"
#include "utils/models/user.h"
#include "utils/serializer.h"
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace {
constexpr std::uint32_t kMaxPayloadBytes = 1024 * 1024;
constexpr std::size_t kMaxEventLog = 256;

bool recvExact(SOCKET socket, char *buffer, int bytes) {
  int received = 0;
  while (received < bytes) {
    int n = recv(socket, buffer + received, bytes - received, 0);
    if (n <= 0)
      return false;
    received += n;
  }
  return true;
}

std::optional<Packet> readPacket(SOCKET socket) {
  char sizeBuf[4];
  if (!recvExact(socket, sizeBuf, 4))
    return std::nullopt;
  std::uint32_t payloadSize =
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[0])) << 24) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[1])) << 16) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[2])) << 8) |
      static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[3]));
  if (payloadSize == 0 || payloadSize > kMaxPayloadBytes)
    return std::nullopt;
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

bool parseHostPort(const std::string &addr, std::string &host, std::uint16_t &port) {
  const auto colon = addr.rfind(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 >= addr.size())
    return false;
  host = addr.substr(0, colon);
  try {
    unsigned long p = std::stoul(addr.substr(colon + 1));
    if (p == 0 || p > 65535)
      return false;
    port = static_cast<std::uint16_t>(p);
    return true;
  } catch (...) {
    return false;
  }
}

// payload: TYPE|eventId|... → eventId (or whole message if no '|')
std::string eventIdFromPacket(const Packet &packet) {
  const auto first = packet.message.find('|');
  if (first == std::string::npos)
    return packet.message;
  const auto second = packet.message.find('|', first + 1);
  if (second == std::string::npos)
    return packet.message.substr(first + 1);
  return packet.message.substr(first + 1, second - first - 1);
}
} // namespace

GossipManager *GossipManager::instance_ = nullptr;

GossipManager::GossipManager(ConnectionManager &connections) : connections(connections) {}

GossipManager::~GossipManager() { stop(); }

void GossipManager::setInstance(GossipManager *instance) { instance_ = instance; }

GossipManager &GossipManager::getInstance() {
  if (!instance_) {
    throw std::runtime_error("GossipManager not set");
  }
  return *instance_;
}

void GossipManager::start() {
  if (!stopped)
    return;
  if (!startListening(config::PEER_PORT))
    return;

  stopped = false;
  acceptThread = std::thread([this] { acceptLoop(); });
  dialThread = std::thread([this] { dialLoop(); });
  antiEntropyThread = std::thread([this] { antiEntropyLoop(); });
  Logger::logInfo("GossipManager", "Started on peer port " + std::to_string(config::PEER_PORT));
}

void GossipManager::stop() {
  if (stopped.exchange(true))
    return;

  stopListening();
  dialCv.notify_all();
  antiEntropyCv.notify_all();

  std::vector<int> sockets;
  {
    std::lock_guard lock(peersMutex);
    for (const auto &entry : peers) {
      sockets.push_back(entry.first);
    }
    peers.clear();
  }
  for (int fd : sockets) {
    closesocket(static_cast<SOCKET>(fd));
  }

  if (acceptThread.joinable())
    acceptThread.join();
  if (dialThread.joinable())
    dialThread.join();
  if (antiEntropyThread.joinable())
    antiEntropyThread.join();

  Logger::logInfo("GossipManager", "Stopped");
}

bool GossipManager::startListening(std::uint16_t port) {
  SOCKET socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketFd == INVALID_SOCKET) {
    Logger::logError("GossipManager", "Failed to create socket");
    return false;
  }

  int reuse = 1;
  if (setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse),
                 sizeof(reuse)) != 0) {
    Logger::logWarning("GossipManager", "setsockopt(SO_REUSEADDR) failed");
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  if (bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    Logger::logError("GossipManager", "Failed to bind port " + std::to_string(port));
    closesocket(socketFd);
    return false;
  }

  if (listen(socketFd, SOMAXCONN) != 0) {
    Logger::logError("GossipManager", "Failed to listen on port " + std::to_string(port));
    closesocket(socketFd);
    return false;
  }

  listeningSocket = static_cast<int>(socketFd);
  Logger::logInfo("GossipManager", "Listening on port " + std::to_string(port));
  return true;
}

void GossipManager::acceptLoop() {
  while (!stopped) {
    SOCKET peer = accept(static_cast<SOCKET>(listeningSocket), nullptr, nullptr);
    if (peer == INVALID_SOCKET)
      break;

    int fd = static_cast<int>(peer);
    Packet hello(config::NODE_ID, "*", Packet::PacketType::GOSSIP_HELLO);
    sendPacket(fd, hello);

    std::thread([this, fd] { handlePeer(fd); }).detach();
  }
}

void GossipManager::dialLoop() {
  while (!stopped) {
    for (const auto &addr : config::PEERS) {
      if (stopped)
        break;

      {
        std::lock_guard lock(peersMutex);
        if (!config::PEERS.empty() && peers.size() >= config::PEERS.size())
          continue;
      }

      std::string host;
      std::uint16_t port = 0;
      if (!parseHostPort(addr, host, port)) {
        Logger::logWarning("GossipManager", "Bad peer address: " + addr);
        continue;
      }

      SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (sock == INVALID_SOCKET)
        continue;

      sockaddr_in address{};
      address.sin_family = AF_INET;
      address.sin_port = htons(port);
      if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        closesocket(sock);
        continue;
      }

      if (connect(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
        closesocket(sock);
        continue;
      }

      const int fd = static_cast<int>(sock);
      Packet hello(config::NODE_ID, "*", Packet::PacketType::GOSSIP_HELLO);
      if (!sendPacket(fd, hello)) {
        closesocket(sock);
        continue;
      }

      Logger::logInfo("GossipManager", "Dialed peer " + addr);
      std::thread([this, fd] { handlePeer(fd); }).detach();
    }

    std::unique_lock lock(dialMutex);
    dialCv.wait_for(lock, std::chrono::seconds(3), [this] { return stopped.load(); });
  }
}

void GossipManager::antiEntropyLoop() {
  while (!stopped) {
    {
      std::unique_lock lock(antiEntropyMutex);
      antiEntropyCv.wait_for(lock, std::chrono::seconds(3), [this] { return stopped.load(); });
    }
    if (stopped)
      break;

    Packet digest(config::NODE_ID, "*", Packet::PacketType::GOSSIP_DIGEST);
    {
      std::lock_guard lock(seenMutex);
      digest.message = buildDigestLocked();
    }

    std::vector<int> sockets;
    {
      std::lock_guard lock(peersMutex);
      sockets.reserve(peers.size());
      for (const auto &entry : peers) {
        sockets.push_back(entry.first);
      }
    }
    for (int fd : sockets) {
      sendPacket(fd, digest);
    }
  }
}

void GossipManager::handlePeer(int peerSocket) {
  SOCKET sock = static_cast<SOCKET>(peerSocket);
  bool registered = false;
  bool reject = false;

  while (!stopped && !reject) {
    std::optional<Packet> packet = readPacket(sock);
    if (!packet)
      break;

    switch (packet->type) {
    case Packet::PacketType::GOSSIP_HELLO: {
      if (!registerPeer(peerSocket, packet->sender)) {
        Logger::logWarning("GossipManager", "Rejected peer HELLO from " + packet->sender);
        reject = true;
        break;
      }
      registered = true;
      Logger::logInfo("GossipManager", "Peer registered: " + packet->sender);
      break;
    }

    case Packet::PacketType::GOSSIP_EVENT: {
      if (!registered)
        break;
      const std::string id = eventIdFromPacket(*packet);
      if (id.empty())
        break;
      {
        std::lock_guard lock(seenMutex);
        if (!seenEvents.insert(id).second)
          break;
        rememberEventLocked(id, *packet);
      }
      applyEvent(*packet);
      Logger::logInfo("GossipManager", "Got event " + id);
      break;
    }

    case Packet::PacketType::GOSSIP_DIGEST: {
      if (!registered)
        break;
      std::vector<std::string> missing;
      {
        std::lock_guard lock(seenMutex);
        std::stringstream ss(packet->message);
        std::string id;
        while (std::getline(ss, id, '|')) {
          if (!id.empty() && seenEvents.find(id) == seenEvents.end()) {
            missing.push_back(id);
          }
        }
      }
      if (missing.empty())
        break;

      Packet pull(config::NODE_ID, packet->sender, Packet::PacketType::GOSSIP_PULL);
      for (std::size_t i = 0; i < missing.size(); ++i) {
        if (i > 0)
          pull.message += '|';
        pull.message += missing[i];
      }
      sendPacket(peerSocket, pull);
      break;
    }

    case Packet::PacketType::GOSSIP_PULL: {
      if (!registered)
        break;
      std::stringstream ss(packet->message);
      std::string id;
      while (std::getline(ss, id, '|')) {
        if (id.empty())
          continue;
        std::optional<Packet> full;
        {
          std::lock_guard lock(seenMutex);
          auto it = eventLog.find(id);
          if (it != eventLog.end()) {
            full = it->second;
          }
        }
        if (!full)
          continue;
        full->type = Packet::PacketType::GOSSIP_EVENT;
        sendPacket(peerSocket, *full);
      }
      break;
    }

    default:
      break;
    }
  }

  removePeer(peerSocket);
  closesocket(sock);
  Logger::logInfo("GossipManager", "Peer disconnected socket " + std::to_string(peerSocket));
}

bool GossipManager::registerPeer(int socket, const std::string &nodeId) {
  if (nodeId == config::NODE_ID)
    return false;
  std::lock_guard lock(peersMutex);
  for (const auto &p : peers) {
    if (p.second == nodeId)
      return false;
  }
  peers[socket] = nodeId;
  return true;
}

void GossipManager::rememberEventLocked(const std::string &id, const Packet &event) {
  eventLog[id] = event;
  recentEventIds.push_back(id);
  while (recentEventIds.size() > kMaxEventLog) {
    eventLog.erase(recentEventIds.front());
    recentEventIds.pop_front();
  }
}

std::string GossipManager::buildDigestLocked() const {
  std::string out;
  for (const auto &id : recentEventIds) {
    if (!out.empty())
      out += '|';
    out += id;
  }
  return out;
}

bool GossipManager::applyEvent(const Packet &event) {
  std::stringstream ss(event.message);
  std::string type;
  std::string eventId;
  std::string username;
  std::string content;
  std::string ts;
  if (!std::getline(ss, type, '|') || !std::getline(ss, eventId, '|') ||
      !std::getline(ss, username, '|') || !std::getline(ss, content, '|') ||
      !std::getline(ss, ts, '|')) {
    Logger::logWarning("GossipManager", "Malformed event payload");
    return false;
  }

  if (type != "MESSAGE") {
    // presence / user events: handled in later steps
    return true;
  }

  std::time_t created = 0;
  try {
    created = static_cast<std::time_t>(std::stoll(ts));
  } catch (...) {
    Logger::logWarning("GossipManager", "Invalid event timestamp");
    return false;
  }

  const std::string roomName = event.room.empty() ? "Lobby" : event.room;
  const User from(username, "", User::UserType::GUEST);
  const Message msg(from, User::anonymousUser(), content, eventId, created);

  const bool inserted = DatabaseManager::getInstance().saveMessage(msg, roomName);
  if (inserted) {
    std::optional<Room> room = RoomManager::getInstance().getRoom(roomName);
    if (room) {
      Packet push(username, "", Packet::PacketType::MESSAGE, roomName, content, 0);
      RoomManager::getInstance().broadcast(*room, push, connections, -1);
    }
  }
  return true;
}

void GossipManager::rumor(const Packet &event) {
  const std::string id = eventIdFromPacket(event);
  if (id.empty())
    return;

  Packet out = event;
  out.type = Packet::PacketType::GOSSIP_EVENT;
  if (out.sender.empty())
    out.sender = config::NODE_ID;
  if (out.receiver.empty())
    out.receiver = "*";

  {
    std::lock_guard lock(seenMutex);
    if (!seenEvents.insert(id).second)
      return;
    rememberEventLocked(id, out);
  }

  applyEvent(out);

  std::vector<int> sockets;
  {
    std::lock_guard lock(peersMutex);
    sockets.reserve(peers.size());
    for (const auto &entry : peers) {
      sockets.push_back(entry.first);
    }
  }

  for (int fd : sockets) {
    if (!sendPacket(fd, out)) {
      Logger::logWarning("GossipManager", "Failed to rumor to socket " + std::to_string(fd));
    }
  }
}

void GossipManager::stopListening() {
  if (listeningSocket != -1) {
    closesocket(static_cast<SOCKET>(listeningSocket));
    listeningSocket = -1;
  }
}

bool GossipManager::sendPacket(int socket, const Packet &packet) {
  std::string framed = Serializer::serialize(packet);
  if (framed.empty())
    return false;

  std::lock_guard lock(sendMutex);
  SOCKET sock = static_cast<SOCKET>(socket);
  int sent = 0;
  const int total = static_cast<int>(framed.size());
  while (sent < total) {
    int n = send(sock, framed.data() + sent, total - sent, 0);
    if (n == SOCKET_ERROR)
      return false;
    sent += n;
  }
  return true;
}

void GossipManager::removePeer(int socket) {
  std::lock_guard lock(peersMutex);
  peers.erase(socket);
}
