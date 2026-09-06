/**
 * Network class
 *
 * @brief Client-side network transport.
 * @date 06-09-2026
 */

#include "client/network.h"
#include "config/config.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/serializer.h"
#include <ctime>
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
  return true;
}

// disconnecting from the server
void Network::disconnect() {
  if (clientSocket != -1) {
    closesocket(static_cast<SOCKET>(clientSocket));
    clientSocket = -1;
  }
  if (winsockStarted) {
    WSACleanup();
    winsockStarted = false;
  }
  if (connected) {
    Logger::logInfo("Network", "Disconnected from server");
  }
  connected = false;
}

// sending a packet to the server
bool Network::sendPacket(const Packet &packet, std::string_view message) {
  std::string serialized = Serializer::serialize(packet);
  if (serialized.empty()) {
    Logger::logError("Network", "Failed to serialize packet");
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connected) {
      std::cout << "Not connected to the server\n";
      return false;
    }

    if (send(clientSocket, serialized.c_str(), static_cast<int>(serialized.size()), 0) ==
        SOCKET_ERROR) {
      Logger::logError("Network", "Failed to send packet to the server");
      return false;
    }
  }

  return true;
}

// receiving a packet from the server
std::optional<Packet> Network::receivePacket(const std::string &serializedPacket) {
  std::optional<Packet> packet = Serializer::deserialize(serializedPacket);

  if (!packet) {
    Logger::logError("Network", "Failed to deserialize packet");
    return std::nullopt;
  }

  std::cout << "Received packet from the server: " << Serializer::serialize(*packet) << std::endl;

  return packet;
}

// check if the network is connected
bool Network::isConnected() const { return connected; }