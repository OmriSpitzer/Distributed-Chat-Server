/**
 * GossipManager class
 *
 * @brief Implements the GossipManager class for server's gossip protocol.
 * @date 12-09-2026
 */

#include "server/gossip_manager.h"
#include "config/config.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/gossip_payload.h"
#include "utils/models/logger.h"
#include "utils/models/message.h"
#include "utils/models/user.h"
#include "utils/socket_io.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <ws2tcpip.h>

// parse host and port from a string
bool GossipManager::parseHostPort(const std::string &addr, std::string &host, std::uint16_t &port) {
  // check if the colon is in the address
  const auto colon = addr.rfind(':');
  if (colon == std::string::npos || colon == 0 || colon + 1 >= addr.size())
    return false;

  // get the host from the address
  host = addr.substr(0, colon);
  try {
    unsigned long p = std::stoul(addr.substr(colon + 1));
    if (p == 0 || p > 65535)
      return false;

    // get the port from the address
    port = static_cast<std::uint16_t>(p);
    return true;
  } catch (...) {
    return false;
  }
}

// get the event id from a packet
std::string GossipManager::eventIdFromPacket(const Packet &packet) {
  return gossip_payload::eventId(packet.message);
}

// constructor
GossipManager::GossipManager(ConnectionManager &connections) : connections(connections) {}

// destructor
GossipManager::~GossipManager() { stop(); }

// start the gossip
void GossipManager::start() {
  // check if the gossip is already started
  if (!stopped)
    return;

  // listen on the peer port
  SOCKET socketFd = socket_io::listenTo(config::PEER_PORT);
  if (socketFd == INVALID_SOCKET) {
    Logger::logError("GossipManager",
                     "Failed to listen on port " + std::to_string(config::PEER_PORT));
    return;
  }

  // start the accept, dial, and anti-entropy threads
  listeningSocket = socketFd;
  stopped = false;
  acceptThread = std::thread([this] { acceptLoop(); });
  dialThread = std::thread([this] { dialLoop(); });
  antiEntropyThread = std::thread([this] { antiEntropyLoop(); });

  Logger::logInfo("GossipManager", "Listening on port " + std::to_string(config::PEER_PORT));
}

// stop the gossip
void GossipManager::stop() {
  // check if the gossip is already stopped
  if (stopped.exchange(true))
    return;

  // stop listening on the peer port
  if (listeningSocket != INVALID_SOCKET) {
    socket_io::close(listeningSocket);
    listeningSocket = INVALID_SOCKET;
  }

  // notify the dial and anti-entropy threads
  dialCv.notify_all();
  antiEntropyCv.notify_all();

  // close all peer sockets (registered or still handshaking)
  std::vector<SOCKET> sockets;
  {
    std::lock_guard lock(peersMutex);
    sockets.assign(openPeerSockets.begin(), openPeerSockets.end());
    openPeerSockets.clear();
    peers.clear();
    outboundAddrs.clear();
  }
  for (SOCKET fd : sockets)
    socket_io::close(fd);

  // join the accept, dial, and anti-entropy threads
  if (acceptThread.joinable())
    acceptThread.join();
  if (dialThread.joinable())
    dialThread.join();
  if (antiEntropyThread.joinable())
    antiEntropyThread.join();

  // join the peer threads
  std::vector<std::thread> toJoin;
  {
    std::lock_guard lock(peerThreadsMutex);
    toJoin.swap(peerThreads);
  }
  for (auto &t : toJoin) {
    if (t.joinable())
      t.join();
  }

  Logger::logInfo("GossipManager", "Stopped");
}

// accept loop
void GossipManager::acceptLoop() {
  while (!stopped) {
    // accept a new peer
    SOCKET peer = socket_io::acceptFrom(listeningSocket);
    if (peer == INVALID_SOCKET)
      break;

    // send a hello packet to the peer
    SOCKET fd = peer;
    {
      std::lock_guard lock(peersMutex);
      openPeerSockets.insert(fd);
    }
    Packet hello(config::NODE_ID, "*", Packet::PacketType::GOSSIP_HELLO);
    sendPacket(fd, hello);

    // create a thread to handle the peer
    spawnPeerHandler(fd);
  }
}

// dial loop
void GossipManager::dialLoop() {
  while (!stopped) {
    // dial all peers
    for (const auto &addr : config::PEERS) {
      if (stopped)
        break;

      // check if we have reached the maximum number of peers
      {
        std::lock_guard lock(peersMutex);
        if (!config::PEERS.empty() && peers.size() >= config::PEERS.size())
          break;

        // check if the address is already in the list of outbound addresses
        bool already = false;
        for (const auto &entry : outboundAddrs) {
          if (entry.second == addr) {
            already = true;
            break;
          }
        }
        if (already)
          continue;
      }

      // parse the host and port from the address
      std::string host;
      std::uint16_t port = 0;
      if (!parseHostPort(addr, host, port)) {
        Logger::logWarning("GossipManager", "Bad peer address: " + addr);
        continue;
      }

      // create a socket to connect to the peer
      SOCKET sock = socket_io::connectTo(host, port);
      if (sock == INVALID_SOCKET)
        continue;

      // send a hello packet to the peer
      const SOCKET fd = sock;
      Packet hello(config::NODE_ID, "*", Packet::PacketType::GOSSIP_HELLO);
      if (!sendPacket(fd, hello)) {
        socket_io::close(sock);
        continue;
      }

      // add the outbound address to the list of outbound addresses
      {
        std::lock_guard lock(peersMutex);
        outboundAddrs[fd] = addr;
        openPeerSockets.insert(fd);
      }

      // create a thread to handle the peer
      spawnPeerHandler(fd);

      Logger::logInfo("GossipManager", "Dialed peer " + addr);
    }

    // wait for the next dial
    std::unique_lock lock(dialMutex);
    dialCv.wait_for(lock, std::chrono::seconds(DIAL_INTERVAL), [this] { return stopped.load(); });
  }
}

// anti-entropy loop
void GossipManager::antiEntropyLoop() {
  while (!stopped) {
    // wait for the next anti-entropy
    {
      std::unique_lock lock(antiEntropyMutex);
      antiEntropyCv.wait_for(lock, std::chrono::seconds(ANTI_ENTROPY_INTERVAL),
                             [this] { return stopped.load(); });
    }
    if (stopped)
      break;

    // build the digest
    Packet digest(config::NODE_ID, "*", Packet::PacketType::GOSSIP_DIGEST);
    {
      std::lock_guard lock(seenMutex);
      digest.message = buildDigestLocked();
    }

    // send the digest to all peers
    std::vector<SOCKET> sockets;
    {
      std::lock_guard lock(peersMutex);
      sockets.reserve(peers.size());
      for (const auto &entry : peers) {
        sockets.push_back(entry.first);
      }
    }
    for (SOCKET fd : sockets) {
      sendPacket(fd, digest);
    }
  }
}

// handle a peer
void GossipManager::handlePeer(SOCKET peerSocket) {
  SOCKET sock = peerSocket;
  bool registered = false;
  bool reject = false;

  while (!stopped && !reject) {
    // read a packet from the peer
    std::optional<Packet> packet = socket_io::readPacket(sock);
    if (!packet)
      break;

    // handle the packet based on the type
    switch (packet->type) {
      // hello packet
    case Packet::PacketType::GOSSIP_HELLO: {
      // register the peer
      if (!registerPeer(peerSocket, packet->sender)) {
        Logger::logWarning("GossipManager", "Rejected peer HELLO from " + packet->sender);
        reject = true;
        break;
      }

      // set the peer as registered
      registered = true;
      Logger::logInfo("GossipManager", "Peer registered: " + packet->sender);
      break;
    }

    // event packet
    case Packet::PacketType::GOSSIP_EVENT: {
      // check if the peer is registered
      if (!registered)
        break;

      // add the event to the seen events
      const std::string id = eventIdFromPacket(*packet);
      if (id.empty())
        break;
      {
        std::lock_guard lock(seenMutex);
        if (!seenEvents.insert(id).second)
          break;
        rememberEventLocked(id, *packet);
      }

      // apply the event
      applyEvent(*packet);
      Logger::logInfo("GossipManager", "Got event " + id);
      break;
    }

    // digest packet
    case Packet::PacketType::GOSSIP_DIGEST: {
      // check if the peer is registered
      if (!registered)
        break;

      // build the missing event ids
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

      // send a pull packet to the peer for the missing event ids
      Packet pull(config::NODE_ID, packet->sender, Packet::PacketType::GOSSIP_PULL);
      for (std::size_t i = 0; i < missing.size(); ++i) {
        if (i > 0)
          pull.message += '|';
        pull.message += missing[i];
      }
      sendPacket(peerSocket, pull);
      break;
    }

    // pull packet
    case Packet::PacketType::GOSSIP_PULL: {
      // check if the peer is registered
      if (!registered)
        break;

      // parse the missing event ids
      std::stringstream ss(packet->message);
      std::string id;
      while (std::getline(ss, id, '|')) {
        if (id.empty())
          continue;

        // find the event in the event log
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

        // send the event to the peer
        full->type = Packet::PacketType::GOSSIP_EVENT;
        sendPacket(peerSocket, *full);
      }
      break;
    }

    // unknown packet
    default:
      break;
    }
  }

  // remove the peer and close the socket (skip close if stop() already did)
  removePeer(peerSocket);
  bool shouldClose = false;
  {
    std::lock_guard lock(peersMutex);
    shouldClose = openPeerSockets.erase(peerSocket) > 0;
  }
  if (shouldClose) {
    socket_io::close(sock);
  }
  Logger::logInfo("GossipManager", "Peer disconnected socket " + std::to_string(peerSocket));
}

// register a peer
bool GossipManager::registerPeer(SOCKET socket, const std::string &nodeId) {
  // check if the peer is the local node
  if (nodeId == config::NODE_ID)
    return false;

  // check if the peer is already registered
  std::lock_guard lock(peersMutex);
  for (const auto &p : peers) {
    if (p.second == nodeId)
      return false;
  }

  // add the peer to the list of peers
  peers[socket] = nodeId;
  return true;
}

// remember an event
void GossipManager::rememberEventLocked(const std::string &id, const Packet &event) {
  // add the event to the event log
  eventLog[id] = event;

  // add the event id to the recent event ids
  recentEventIds.push_back(id);
  while (recentEventIds.size() > MAX_EVENT_LOG) {
    const std::string old = recentEventIds.front();
    recentEventIds.pop_front();
    eventLog.erase(old);
    seenEvents.erase(old);
  }
}

// build the digest
std::string GossipManager::buildDigestLocked() const {
  std::string out;
  for (const auto &id : recentEventIds) {
    if (!out.empty())
      out += '|';
    out += id;
  }
  return out;
}

// apply an event
bool GossipManager::applyEvent(const Packet &event) {
  auto &db = DatabaseManager::getInstance();

  // parse the event
  const auto fields = gossip_payload::decode(event.message);
  if (!fields) {
    Logger::logWarning("GossipManager", "Malformed event payload");
    return false;
  }

  const std::string &type = fields->type;
  const std::string &eventId = fields->eventId;
  const std::string &username = fields->username;
  const std::string &content = fields->content;
  const std::string &ts = fields->field5;

  // login event
  if (type == "LOGIN") {
    // check if the username and content are empty
    if (username.empty() || content.empty()) {
      Logger::logWarning("GossipManager", "Malformed LOGIN event");
      return false;
    }

    // set the user online
    db.setOnline(username, content);
    Logger::logInfo("GossipManager", "Applied LOGIN for " + username + " on " + content);
    return true;
  }

  // logout event
  if (type == "LOGOUT") {
    // check if the username is empty
    if (username.empty()) {
      Logger::logWarning("GossipManager", "Malformed LOGOUT event");
      return false;
    }

    // clear the user online and all membership
    db.clearOnline(username);
    db.clearAllMembership(username);
    Logger::logInfo("GossipManager", "Applied LOGOUT for " + username);
    return true;
  }

  // user created event
  if (type == "USER_CREATED") {
    // check if the username, content, and timestamp are empty
    if (username.empty() || content.empty() || ts.empty()) {
      Logger::logWarning("GossipManager", "Malformed USER_CREATED event");
      return false;
    }

    // create the user if it doesn't exist
    if (!db.userExists(username)) {
      try {
        db.createUser(username, content, ts);
      } catch (const std::exception &e) {
        Logger::logWarning("GossipManager", std::string("USER_CREATED apply failed for ") +
                                                username + ": " + e.what());
        return false;
      }
    }
    Logger::logInfo("GossipManager", "Applied USER_CREATED for " + username);
    return true;
  }

  // room join event
  if (type == "ROOM_JOIN") {
    const std::string newRoom = event.room.empty() ? "Lobby" : event.room;
    const std::string &nodeId = content;
    const std::string &prevRoom = ts;

    // check if the username and node id are empty
    if (username.empty() || nodeId.empty()) {
      Logger::logWarning("GossipManager", "Malformed ROOM_JOIN event");
      return false;
    }

    auto resolveId = [](const std::string &name) -> std::optional<int> {
      auto room = RoomManager::getInstance().getRoom(name);
      if (!room) {
        return std::nullopt;
      }
      return room->getId();
    };

    const auto newId = resolveId(newRoom);
    if (!newId) {
      Logger::logWarning("GossipManager", "ROOM_JOIN unknown room: " + newRoom);
      return false;
    }

    // clear the membership if the previous room is not the same as the new room
    if (!prevRoom.empty() && prevRoom != newRoom) {
      if (const auto prevId = resolveId(prevRoom)) {
        db.clearMembership(username, *prevId);
      }
    }

    // set the membership
    db.setMembership(username, *newId, nodeId);
    Logger::logInfo("GossipManager",
                    "Applied ROOM_JOIN for " + username + " -> " + newRoom + " on " + nodeId);
    return true;
  }

  // room leave event
  if (type == "ROOM_LEAVE") {
    const std::string roomName = !event.room.empty() ? event.room : ts;

    // check if the username and room name are empty
    if (username.empty() || roomName.empty()) {
      Logger::logWarning("GossipManager", "Malformed ROOM_LEAVE event");
      return false;
    }

    auto room = RoomManager::getInstance().getRoom(roomName);
    if (!room) {
      Logger::logWarning("GossipManager", "ROOM_LEAVE unknown room: " + roomName);
      return false;
    }

    // clear the membership
    db.clearMembership(username, room->getId());
    Logger::logInfo("GossipManager", "Applied ROOM_LEAVE for " + username + " from " + roomName);
    return true;
  }

  // message event
  if (type != "MESSAGE") {
    return true;
  }

  // build the message
  std::time_t created = 0;
  try {
    created = static_cast<std::time_t>(std::stoll(ts));
  } catch (...) {
    Logger::logWarning("GossipManager", "Invalid event timestamp");
    return false;
  }
  const std::string roomName = event.room.empty() ? "Lobby" : event.room;
  auto room = RoomManager::getInstance().getRoom(roomName);
  if (!room) {
    Logger::logWarning("GossipManager", "MESSAGE unknown room: " + roomName);
    return false;
  }

  // resolve sender from DB so email FK on messages.sender_email is satisfied
  std::optional<User> sender = db.getUser(username);
  if (!sender) {
    Logger::logWarning("GossipManager", "MESSAGE unknown sender: " + username);
    return false;
  }
  const Message msg(*sender, User::anonymousUser(), content, eventId, created);

  // save the message
  bool inserted = false;
  try {
    inserted = db.saveMessage(msg, room->getId());
  } catch (const std::exception &e) {
    Logger::logWarning("GossipManager",
                       std::string("MESSAGE apply failed for ") + eventId + ": " + e.what());
    return false;
  }

  // broadcast the message to the room
  if (inserted) {
    Packet push(username, "", Packet::PacketType::MESSAGE, roomName, content, 0);
    RoomManager::getInstance().broadcast(*room, push, connections, -1);
  }
  return true;
}

// rumor a packet
void GossipManager::rumor(const Packet &event) {
  const std::string id = eventIdFromPacket(event);

  // check if the event id is empty
  if (id.empty())
    return;

  // build the output packet
  Packet out = event;
  out.type = Packet::PacketType::GOSSIP_EVENT;
  if (out.sender.empty())
    out.sender = config::NODE_ID;
  if (out.receiver.empty())
    out.receiver = "*";

  // add the event id to the seen events
  {
    std::lock_guard lock(seenMutex);
    if (!seenEvents.insert(id).second)
      return;
    rememberEventLocked(id, out);
  }

  // apply the event
  applyEvent(out);

  // send the packet to all peers
  std::vector<SOCKET> sockets;
  {
    std::lock_guard lock(peersMutex);
    sockets.reserve(peers.size());
    for (const auto &entry : peers) {
      sockets.push_back(entry.first);
    }
  }

  // send the packet to all peers
  for (SOCKET fd : sockets) {
    if (!sendPacket(fd, out)) {
      Logger::logWarning("GossipManager", "Failed to rumor to socket " + std::to_string(fd));
    }
  }
}

// send a packet
bool GossipManager::sendPacket(SOCKET socket, const Packet &packet) {
  std::lock_guard lock(sendMutex);
  return socket_io::writePacket(socket, packet);
}

// remove a peer
void GossipManager::removePeer(SOCKET socket) {
  std::lock_guard lock(peersMutex);
  peers.erase(socket);
  outboundAddrs.erase(socket);
}

// spawn a peer handler
void GossipManager::spawnPeerHandler(SOCKET fd) {
  std::lock_guard lock(peerThreadsMutex);
  peerThreads.emplace_back([this, fd] { handlePeer(fd); });
}
