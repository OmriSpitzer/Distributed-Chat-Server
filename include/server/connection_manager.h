/**
 * ConnectionManager header file class
 *
 * @date 06-09-2026
 */
#pragma once
#include "server/client_session.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

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

  // stop accepting packets
  void stopAccepting();

private:
  int listeningSocket; // listening socket file descriptor
  std::unordered_map<int, std::shared_ptr<ClientSession>> sessions; // sessions
  bool listening;                                                   // is the server listening
  mutable std::mutex sessionsMutex;                                 // sessions mutex

  // handle the client
  void handleClient(int clientSocket);

  // add new session
  void addSession(int socket, std::shared_ptr<ClientSession> session);

  // remove session
  void removeSession(int socket);
};
