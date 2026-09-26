/**
 * Gossip Manager class header file
 *
 * @date 12-09-2026
 */

#pragma once
#include "server/connection_manager.h"
#include "utils/models/client_endpoint.h"
#include "utils/models/packet.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

class GossipManager {
public:
  // constructor
  explicit GossipManager(ConnectionManager &connections);

  // destructor
  ~GossipManager();

  // start the gossip
  void start();

  // stop the gossip
  void stop();

  // delete copy constructor and assignment operator
  GossipManager(const GossipManager &) = delete;
  GossipManager &operator=(const GossipManager &) = delete;

  // rumor a packet
  void rumor(const Packet &packet);

  // live client-facing endpoints of cluster members (self + HELLO-advertised peers)
  std::unordered_map<std::string, ClientEndpoint> getClientPeers() const;

  // serialize live client endpoints for SERVER_DIRECTORY (endpoint(...);...)
  std::string buildServerDirectory() const;

private:
  ConnectionManager &connections;         // local client sockets for apply
  SOCKET listeningSocket{INVALID_SOCKET}; // listening socket
  std::atomic<bool> stopped{true};        // stopped listening

  std::thread acceptThread;      // accept thread
  std::thread dialThread;        // dial thread
  std::thread antiEntropyThread; // anti-entropy thread

  std::unordered_map<SOCKET, std::string> outboundAddrs;       // socket -> "host:port"
  std::unordered_map<std::string, ClientEndpoint> clientPeers; // nodeId -> client peer

  std::unordered_set<std::string> seenEvents;       // seen events
  std::deque<std::string> recentEventIds;           // ordered ids for digests
  std::unordered_map<std::string, Packet> eventLog; // id -> full event for PULL
  std::unordered_set<SOCKET> openPeerSockets;       // all sockets with a live handlePeer

  mutable std::mutex peersMutex;         // peers mutex
  std::mutex sendMutex;                  // send mutex
  std::mutex seenMutex;                  // seen / eventLog mutex
  std::condition_variable dialCv;        // dial condition variable
  std::mutex dialMutex;                  // dial mutex
  std::condition_variable antiEntropyCv; // anti-entropy condition variable
  std::mutex antiEntropyMutex;           // anti-entropy mutex

  static constexpr std::size_t MAX_EVENT_LOG =
      256; // maximum number of events to keep in the event log
  static constexpr std::size_t DIAL_INTERVAL = 3; // interval in seconds to dial peers
  static constexpr std::size_t ANTI_ENTROPY_INTERVAL =
      3; // interval in seconds to send anti-entropy packets

  std::vector<std::thread> peerThreads; // peer threads
  std::mutex peerThreadsMutex;          // peer threads mutex

  // spawn a peer handler
  void spawnPeerHandler(SOCKET fd);

  // accept loop
  void acceptLoop();

  // dial loop
  void dialLoop();

  // anti-entropy: periodic GOSSIP_DIGEST
  void antiEntropyLoop();

  // handle peer
  void handlePeer(SOCKET peerSocket);

  // send a packet to a peer
  bool sendPacket(SOCKET socket, const Packet &packet);

  // remove a peer
  void removePeer(SOCKET socket);

  // register a peer and track its client listen endpoint (host/port may be empty if HELLO omitted
  // them)
  bool registerPeer(SOCKET socket, const std::string &nodeId, const std::string &host,
                    std::uint16_t port);

  // sockets of remote peers that can receive gossip (excludes local self entry)
  std::vector<SOCKET> peerSocketsLocked() const;

  // build SERVER_DIRECTORY body (caller holds peersMutex)
  std::string buildServerDirectoryLocked() const;

  // push updated directory to all connected chat clients
  void notifyServerDirectoryChanged();

  // INSERT OR IGNORE + local broadcast for chat MESSAGE events
  bool applyEvent(const Packet &event);

  // remember event for digest / pull (caller holds seenMutex)
  void rememberEventLocked(const std::string &id, const Packet &event);

  // build digest payload from recentEventIds (caller holds seenMutex)
  std::string buildDigestLocked() const;

  // parse host and port from a string
  static bool parseHostPort(const std::string &addr, std::string &host, std::uint16_t &port);

  // get the event id from a packet
  static std::string eventIdFromPacket(const Packet &packet);
};
