/**
 * ConnectionManager header file class
 *
 * @date 14-07-2026
 */
#pragma once
#include "server/client_session.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

class ConnectionManager {
public:
  ConnectionManager();
  ~ConnectionManager();

  // create, bind, and listen on the given port
  bool startListening(std::uint16_t port);

  // close the listening socket and mark as stopped
  void stopListening();

  // getters
  std::uintptr_t getListeningSocket() const;
  bool isListening() const;

  // delete copy
  ConnectionManager(const ConnectionManager &) = delete;
  ConnectionManager &operator=(const ConnectionManager &) = delete;

private:
  // listening socket (platform SOCKET / fd stored as integer)
  std::uintptr_t listeningSocket;

  // sessions
  std::unordered_map<int, std::shared_ptr<ClientSession>> sessions;

  // session mutex
  std::mutex sessionMutex;

  // is the server listening
  bool running;
};
