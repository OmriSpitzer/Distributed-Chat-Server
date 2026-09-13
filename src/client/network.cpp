/**
 * Network class
 *
 * @brief Client-side network transport.
 * @date 13-09-2026
 */

#include "client/network.h"
#include "config/config.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/socket_io.h"
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

// destructor
Network::~Network() { disconnect(); }

// connecting to the server
bool Network::connect() {
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
  SOCKET socketFd = socket_io::connectTo(config::SERVER_HOST, config::PORT);
  if (socketFd == INVALID_SOCKET) {
    Logger::logError("Network", "Failed to connect to " + config::SERVER_HOST + ":" +
                                    std::to_string(config::PORT));
    WSACleanup();
    winsockStarted = false;
    return false;
  }

  // set the client socket
  clientSocket = static_cast<int>(socketFd);
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

  int fd = -1;
  {
    std::lock_guard<std::mutex> lock(mutex);
    fd = clientSocket;
    clientSocket = -1;
  }

  // close the client socket
  if (fd != -1) {
    closesocket(static_cast<SOCKET>(fd));
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

  // log the disconnection
  if (wasConnected) {
    Logger::logInfo("Network", "Disconnected from server");
  }
}

// sending a packet to the server
bool Network::sendPacket(const Packet &packet) {
  // check if connected and the client socket is valid
  std::lock_guard<std::mutex> lock(mutex);
  if (!connected || clientSocket == -1) {
    Logger::logError("Network", "Not connected to the server");
    return false;
  }

  // send the packet to the server
  if (!socket_io::writePacket(static_cast<SOCKET>(clientSocket), packet)) {
    const int fd = clientSocket;
    clientSocket = -1;
    connected = false;
    closesocket(static_cast<SOCKET>(fd));
    incomingCv.notify_all();

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
    int fd = -1;
    {
      std::lock_guard<std::mutex> lock(mutex);
      fd = clientSocket;
    }
    if (fd == -1) {
      break;
    }

    // read the packet from the server
    std::optional<Packet> packet = socket_io::readPacket(static_cast<SOCKET>(fd));
    if (!packet) {
      connected = false;
      incomingCv.notify_all();
      Logger::logError("Network", "Failed to read packet from the server");
      break;
    }

    // check if the packet is a heartbeat
    if (packet->type == Packet::PacketType::HEARTBEAT) {
      if (packet->message == "ping") {
        Packet pong("client", "server", Packet::PacketType::HEARTBEAT, "", "pong");
        if (!sendPacket(pong))
          break;
      }
      continue;
    }

    // check if the packet is a message
    if (packet->type == Packet::PacketType::MESSAGE && packet->responseCode == 0) {
      Logger::logInfo("Network", "[" + packet->sender + "]: " + packet->message);
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
