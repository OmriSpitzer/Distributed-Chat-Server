/**
 * ConnectionManager header file class
 *
 * @date 11-09-2026
 */
#pragma once
#include "server/client_session.h"
#include "utils/models/packet.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

// forward declaration of GossipManager
class GossipManager;

class ConnectionManager {
public:
  // constructor
  ConnectionManager();

  // destructor
  ~ConnectionManager();

  // create, bind, and listen on the given port
  bool startListening(std::uint16_t port);

  // close the listening socket and mark as stopped
  void stopListening();

  // getters
  SOCKET getListeningSocket() const;
  bool isListening() const;
  std::unordered_map<SOCKET, std::shared_ptr<ClientSession>> getSessions() const;

  // delete copy
  ConnectionManager(const ConnectionManager &) = delete;
  ConnectionManager &operator=(const ConnectionManager &) = delete;

  // accept loop
  void acceptLoop();

  // session + Lobby + welcome, without starting a TCP read loop
  bool openSession(SOCKET fd);

  // touch, process, and reply. false means the caller should drop the socket
  bool dispatchPacket(SOCKET fd, const Packet &packet);

  // later sendPacket calls wrap this socket in a binary WebSocket frame
  void markWebSocket(SOCKET socket);

  // close the socket and drop the session
  void releaseClient(SOCKET socket);

  // does the user have a session
  bool hasSession(const User &user) const;

  // send a packet to a connected client
  bool sendPacket(SOCKET socket, const Packet &packet);

  // close a client socket so its read loop exits
  void closeClient(SOCKET socket);

  // join handleClient threads (safe to call more than once)
  void joinClientThreads();

  // inject the gossip manager owned by Server (null when stopped)
  void setGossip(GossipManager *gossip);

  // fan-out via injected gossip; no-op if not set
  void rumor(const Packet &event);

  // push SERVER_DIRECTORY to one client (only != INVALID) or all sessions
  void pushServerDirectory(const std::string &body, SOCKET only = INVALID_SOCKET);

  // push this node's live client-endpoint directory (no-op if gossip not wired)
  void refreshServerDirectory(SOCKET only = INVALID_SOCKET);

private:
  GossipManager *gossip_{nullptr}; // gossip manager owned by Server

  std::unordered_map<SOCKET, std::shared_ptr<ClientSession>> sessions; // sessions
  std::unordered_set<SOCKET> webSockets_;                              // browser sockets
  SOCKET listeningSocket;                                              // listening socket
  std::atomic<bool> listening{false};                                  // is the server listening
  mutable std::mutex sessionsMutex;                                    // sessions mutex
  std::vector<std::thread> clientThreads;                              // handleClient threads
  std::mutex clientThreadsMutex;                                       // clientThreads mutex

  // handle the client
  void handleClient(SOCKET clientSocket);

  // track a handleClient thread, or close the socket if stop already won
  void spawnClientHandler(SOCKET fd);

  // add new session
  void addSession(SOCKET socket, std::shared_ptr<ClientSession> session);

  // remove session
  void removeSession(SOCKET socket);
};
