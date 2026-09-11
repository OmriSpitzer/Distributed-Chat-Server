/**
 * ClientSession class
 *
 * @brief Holds the per-client session state for a single client.
 * @date 11-09-2026
 */

#include "server/client_session.h"
#include "config/config.h"
#include <chrono>
#include <mutex>

// constructor
ClientSession::ClientSession(int socket, const User &user, const Room &room)
    : clientSocket(socket), user(user), room(room), authenticated(false),
      lastHeartbeatTime(std::chrono::steady_clock::now()) {}

// getters
int ClientSession::getSocket() const { return clientSocket; }
User ClientSession::getUser() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return user;
}
Room ClientSession::getRoom() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return room;
}
bool ClientSession::isAuthenticated() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  return authenticated;
}
bool ClientSession::isClosed() const { return closed_.load(); }
std::mutex &ClientSession::sendMutex() { return sendMutex_; }

// setters
void ClientSession::setUser(const User &newUser) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  user = newUser;
}
void ClientSession::setRoom(const Room &newRoom) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  room = newRoom;
}
void ClientSession::setAuthenticated(bool value) {
  std::lock_guard<std::mutex> lock(stateMutex_);
  authenticated = value;
}

// mark the client session as closed
bool ClientSession::markClosed() {
  bool expected = false;
  return closed_.compare_exchange_strong(expected, true);
}

// touch the last heartbeat time
void ClientSession::touch() {
  std::lock_guard<std::mutex> lock(stateMutex_);
  lastHeartbeatTime = std::chrono::steady_clock::now();
}

// is the client alive
bool ClientSession::isAlive() const {
  std::lock_guard<std::mutex> lock(stateMutex_);
  auto now = std::chrono::steady_clock::now();
  auto interval = std::chrono::milliseconds(config::HEARTBEAT_TIMEOUT);

  return now - lastHeartbeatTime < interval;
}