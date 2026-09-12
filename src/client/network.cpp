/**
 * Network class
 *
 * @brief Client-side network transport.
 * @date 07-09-2026
 */

#include "client/network.h"
#include "config/config.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/serializer.h"
#include "utils/socket_io.h"
#include <cstdint>
#include <iostream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

// destructor
Network::~Network() { disconnect(); }

// connecting to the server
bool Network::connect() {
  if (connected) {
    Logger::logWarning("Network", "Already connected");
    return false;
  }

  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    Logger::logError("Network", "WSAStartup failed");
    return false;
  }
  winsockStarted = true;

  SOCKET socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketFd == INVALID_SOCKET) {
    Logger::logError("Network", "Failed to create socket");
    WSACleanup();
    winsockStarted = false;
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(config::PORT);
  if (inet_pton(AF_INET, config::SERVER_HOST.c_str(), &address.sin_addr) != 1) {
    Logger::logError("Network", "Invalid server address " + config::SERVER_HOST);
    closesocket(socketFd);
    WSACleanup();
    winsockStarted = false;
    return false;
  }

  if (::connect(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    Logger::logError("Network", "Failed to connect to " + config::SERVER_HOST + ":" +
                                    std::to_string(config::PORT));
    closesocket(socketFd);
    WSACleanup();
    winsockStarted = false;
    return false;
  }

  clientSocket = static_cast<int>(socketFd);
  connected = true;
  readerThread = std::thread([this] { readerLoop(); });
  return true;
}

// disconnecting from the server
void Network::disconnect() {
  const bool wasConnected = connected.exchange(false);
  incomingCv.notify_all();

  int fd = -1;
  {
    std::lock_guard<std::mutex> lock(mutex);
    fd = clientSocket;
    clientSocket = -1;
  }
  if (fd != -1) {
    closesocket(static_cast<SOCKET>(fd));
  }

  if (readerThread.joinable()) {
    readerThread.join();
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    while (!incoming.empty()) {
      incoming.pop();
    }
  }

  if (winsockStarted) {
    WSACleanup();
    winsockStarted = false;
  }
  if (wasConnected) {
    Logger::logInfo("Network", "Disconnected from server");
  }
}

// sending a packet to the server
bool Network::sendPacket(const Packet &packet, std::string_view message) {
  (void)message;
  std::string serialized = Serializer::serialize(packet);
  if (serialized.empty()) {
    Logger::logError("Network", "Failed to serialize packet");
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex);
  if (!connected || clientSocket == -1) {
    std::cout << "Not connected to the server\n";
    return false;
  }

  if (!socket_io::sendExact(static_cast<SOCKET>(clientSocket), serialized.c_str(),
                            static_cast<int>(serialized.size()))) {
    Logger::logError("Network", "Failed to send packet to the server");
    return false;
  }

  return true;
}

// receiving a packet from the server
std::optional<Packet> Network::receivePacket() {
  std::unique_lock<std::mutex> lock(mutex);
  incomingCv.wait(lock, [this] { return !incoming.empty() || !connected; });
  if (incoming.empty()) {
    return std::nullopt;
  }

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

    std::optional<Packet> packet = socket_io::readPacket(static_cast<SOCKET>(fd));
    if (!packet) {
      connected = false;
      incomingCv.notify_all();
      break;
    }

    if (packet->type == Packet::PacketType::HEARTBEAT) {
      if (packet->message == "ping") {
        Packet pong("client", "server", Packet::PacketType::HEARTBEAT, "", "pong");
        sendPacket(pong, "Heartbeat pong");
      }
      continue;
    }

    if (packet->type == Packet::PacketType::MESSAGE && packet->responseCode == 0) {
      std::cout << "\n[" << packet->sender << "]: " << packet->message << std::endl;
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      incoming.push(*packet);
    }
    incomingCv.notify_one();
  }
}

// check if the network is connected
bool Network::isConnected() const { return connected; }
