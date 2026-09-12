/**
 * GossipManager header file class
 *
 * @date 12-09-2026
 */

#pragma once
#include "server/connection_manager.h"
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

private:
  ConnectionManager &connections;  // local client sockets for apply
  int listeningSocket{-1};         // listening socket
  std::atomic<bool> stopped{true}; // stopped listening

  std::thread acceptThread;      // accept thread
  std::thread dialThread;        // dial thread
  std::thread antiEntropyThread; // anti-entropy thread

  std::unordered_map<int, std::string> peers;         // socket -> remote NODE_ID
  std::unordered_set<std::string> seenEvents;         // seen events
  std::deque<std::string> recentEventIds;             // ordered ids for digests
  std::unordered_map<std::string, Packet> eventLog;   // id -> full event for PULL
  std::unordered_map<int, std::string> outboundAddrs; // socket -> "host:port"

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
  void spawnPeerHandler(int fd);

  // accept loop
  void acceptLoop();

  // dial loop
  void dialLoop();

  // anti-entropy: periodic GOSSIP_DIGEST
  void antiEntropyLoop();

  // handle peer
  void handlePeer(int peerSocket);

  // send a packet to a peer
  bool sendPacket(int socket, const Packet &packet);

  // remove a peer
  void removePeer(int socket);

  // register a peer
  bool registerPeer(int socket, const std::string &nodeId);

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
