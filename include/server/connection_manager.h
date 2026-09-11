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
#include <unordered_map>

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
  int getListeningSocket() const;
  bool isListening() const;
  std::unordered_map<int, std::shared_ptr<ClientSession>> getSessions() const;

  // delete copy
  ConnectionManager(const ConnectionManager &) = delete;
  ConnectionManager &operator=(const ConnectionManager &) = delete;

  // accept loop
  void acceptLoop();

  // does the user have a session
  bool hasSession(const User &user) const;

  // send a packet to a connected client
  bool sendPacket(int socket, const Packet &packet);

  // close a client socket so its read loop exits
  void closeClient(int socket);

  // inject the gossip manager owned by Server (null when stopped)
  void setGossip(GossipManager *gossip);

  // fan-out via injected gossip; no-op if not set
  void rumor(const Packet &event);

private:
  GossipManager *gossip_{nullptr}; // gossip manager owned by Server

  std::unordered_map<int, std::shared_ptr<ClientSession>> sessions; // sessions
  int listeningSocket;                                              // listening socket
  std::atomic<bool> listening{false};                               // is the server listening
  mutable std::mutex sessionsMutex;                                 // sessions mutex

  // handle the client
  void handleClient(int clientSocket);

  // add new session
  void addSession(int socket, std::shared_ptr<ClientSession> session);

  // remove session
  void removeSession(int socket);
};
