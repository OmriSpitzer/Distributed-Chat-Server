/**
 * ConnectionManager header file class
 *
 * @date 03-09-2026
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

  // delete copy
  ConnectionManager(const ConnectionManager &) = delete;
  ConnectionManager &operator=(const ConnectionManager &) = delete;

private:
  int listeningSocket; // listening socket file descriptor
  std::unordered_map<int, std::shared_ptr<ClientSession>> sessions; // sessions
  bool listening;                                                   // is the server listening
};
