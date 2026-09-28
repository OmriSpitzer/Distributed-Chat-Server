/**
 * Network class
 *
 * @brief Client-side network transport.
 * @date 13-09-2026
 */

#include "client/network.h"
#include "config/config.h"
#include "utils/logger/logger.h"
#include "utils/models/packet.h"
#include "utils/socket_io.h"
#include <chrono>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

// destructor
Network::~Network() { disconnect(); }

// connecting to config::SERVER_HOST:PORT
bool Network::connect() { return connect(config::SERVER_HOST, config::PORT); }

// connecting to a specific chat endpoint
bool Network::connect(std::string_view host, std::uint16_t port) {
  // check if already connected
  if (connected) {
    Logger::logWarning("Network", "Already connected");
    return false;
  }

  // start winsock
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    Logger::logError("Network", "WSAStartup failed");
    return false;
  }
  winsockStarted = true;

  // connect to the server
  SOCKET socketFd = socket_io::connectTo(host, port);
  if (socketFd == INVALID_SOCKET) {
    Logger::logError("Network", "Failed to connect to " + std::string(host) + ":" +
                                    std::to_string(port));
    WSACleanup();
    winsockStarted = false;
    return false;
  }

  // set the client socket; stamp inbound so the first ping has a full timeout
  clientSocket = socketFd;
  noteInbound();
  connected = true;

  // start the reader thread
  readerThread = std::thread([this] { readerLoop(); });
  return true;
}

// disconnecting from the server
void Network::disconnect() {
  // check if already disconnected
  const bool wasConnected = connected.exchange(false);
  incomingCv.notify_all();

  SOCKET fd = INVALID_SOCKET;
  {
    std::lock_guard<std::mutex> lock(mutex);
    fd = clientSocket;
    clientSocket = INVALID_SOCKET;
  }

  // close the client socket
  if (fd != INVALID_SOCKET) {
    socket_io::close(fd);
  }

  // join the reader thread
  if (readerThread.joinable()) {
    readerThread.join();
  }

  // clear the incoming queue
  {
    std::lock_guard<std::mutex> lock(mutex);
    while (!incoming.empty()) {
      incoming.pop();
    }
  }

  // clean up winsock
  if (winsockStarted) {
    WSACleanup();
    winsockStarted = false;
  }

  lastInboundMs.store(0, std::memory_order_relaxed);

  // log the disconnection
  if (wasConnected) {
    Logger::logInfo("Network", "Disconnected from server");
  }
}

// sending a packet to the server
bool Network::sendPacket(const Packet &packet) {
  SOCKET failedFd = INVALID_SOCKET;
  {
    // check if connected and the client socket is valid
    std::lock_guard<std::mutex> lock(mutex);
    if (!connected || clientSocket == INVALID_SOCKET) {
      Logger::logError("Network", "Not connected to the server");
      return false;
    }

    // send the packet to the server
    if (!socket_io::writePacket(clientSocket, packet)) {
      failedFd = clientSocket;
      clientSocket = INVALID_SOCKET;
    }
  }

  if (failedFd != INVALID_SOCKET) {
    socket_io::close(failedFd);
    markLinkDown();
    Logger::logError("Network", "Failed to send packet to the server");
    return false;
  }

  return true;
}

// receiving a packet from the server
std::optional<Packet> Network::receivePacket() {
  // check if the incoming queue is not empty or the connection is lost
  std::unique_lock<std::mutex> lock(mutex);
  incomingCv.wait(lock, [this] { return !incoming.empty() || !connected; });
  if (incoming.empty()) {
    return std::nullopt;
  }

  // get the packet from the front of the queue
  Packet packet = incoming.front();
  incoming.pop();
  return packet;
}

// read loop: pong heartbeats, queue everything else
void Network::readerLoop() {
  while (connected) {
    SOCKET fd = INVALID_SOCKET;
    {
      std::lock_guard<std::mutex> lock(mutex);
      fd = clientSocket;
    }
    if (fd == INVALID_SOCKET) {
      break;
    }

    // read the packet from the server
    std::optional<Packet> packet = socket_io::readPacket(fd);
    if (!packet) {
      markLinkDown();
      Logger::logError("Network", "Failed to read packet from the server");
      break;
    }
    noteInbound();

    // check if the packet is a heartbeat
    if (packet->type == Packet::PacketType::HEARTBEAT) {
      if (packet->message == "ping") {
        Packet pong("client", "server", Packet::PacketType::HEARTBEAT, "", "pong");
        if (!sendPacket(pong))
          break;
      }
      continue;
    }

    // unsolicited chat / room / server-directory pushes
    if (packet->responseCode == 0 &&
        (packet->type == Packet::PacketType::MESSAGE ||
         packet->type == Packet::PacketType::ROOM_LIST ||
         packet->type == Packet::PacketType::SERVER_DIRECTORY)) {
      if (packet->type == Packet::PacketType::MESSAGE) {
        Logger::logInfo("Network", "[" + packet->sender + "]: " + packet->message);
      }

      PushHandler handler;
      {
        std::lock_guard<std::mutex> lock(mutex);
        handler = pushHandler;
      }
      if (handler) {
        handler(*packet);
      } else if (packet->type == Packet::PacketType::ROOM_LIST) {
        Logger::logWarning("Network", "ROOM_LIST ignored — no push handler");
      } else if (packet->type == Packet::PacketType::SERVER_DIRECTORY) {
        Logger::logWarning("Network", "SERVER_DIRECTORY ignored — no push handler");
      }
      continue;
    }

    // add the packet to the incoming queue
    {
      std::lock_guard<std::mutex> lock(mutex);
      incoming.push(*packet);
    }
    incomingCv.notify_one();
  }
}

// check if the network is connected
bool Network::isConnected() const { return connected; }

// true when the socket is up and no packet has arrived within HEARTBEAT_TIMEOUT
bool Network::heartbeatMissed() const {
  if (!connected.load(std::memory_order_acquire)) {
    return false;
  }
  const auto last = lastInboundMs.load(std::memory_order_relaxed);
  if (last <= 0) {
    return true;
  }
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  return now - last > config::HEARTBEAT_TIMEOUT;
}

// stamp now as the last time the peer sent a packet
void Network::noteInbound() {
  const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
  lastInboundMs.store(now, std::memory_order_relaxed);
}

// mark the socket down and wake the failover loop once
void Network::markLinkDown() {
  const bool wasUp = connected.exchange(false);
  incomingCv.notify_all();
  if (!wasUp) {
    return;
  }

  std::function<void()> handler;
  {
    std::lock_guard<std::mutex> lock(mutex);
    handler = linkDownHandler;
  }
  if (handler) {
    handler();
  }
}

// set push handler for unsolicited packets
void Network::setPushHandler(PushHandler handler) {
  std::lock_guard<std::mutex> lock(mutex);
  pushHandler = std::move(handler);
}

// fired once when the socket drops (peer close or failed send)
void Network::setLinkDownHandler(std::function<void()> handler) {
  std::lock_guard<std::mutex> lock(mutex);
  linkDownHandler = std::move(handler);
}
