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
#include <cstdint>
#include <iostream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

namespace {
constexpr std::uint32_t kMaxPayloadBytes = 1024 * 1024; // same as Serializer

bool recvExact(SOCKET socket, char *buffer, int bytes) {
  int received = 0;
  while (received < bytes) {
    int n = recv(socket, buffer + received, bytes - received, 0);
    if (n <= 0) {
      return false;
    }
    received += n;
  }
  return true;
}
} // namespace

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
std::optional<Packet> Network::receivePacket() {
  std::string framed;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!connected || clientSocket == -1) {
      Logger::logError("Network", "Not connected to the server");
      return std::nullopt;
    }

    SOCKET sock = static_cast<SOCKET>(clientSocket);
    char sizeBuf[4];
    if (!recvExact(sock, sizeBuf, 4)) {
      Logger::logError("Network", "Failed to receive packet size");
      return std::nullopt;
    }

    std::uint32_t payloadSize =
        (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[0])) << 24) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[1])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[2])) << 8) |
        static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[3]));

    if (payloadSize == 0 || payloadSize > kMaxPayloadBytes) {
      Logger::logError("Network", "Invalid packet size");
      return std::nullopt;
    }

    framed.assign(4 + payloadSize, '\0');
    framed[0] = sizeBuf[0];
    framed[1] = sizeBuf[1];
    framed[2] = sizeBuf[2];
    framed[3] = sizeBuf[3];

    if (!recvExact(sock, framed.data() + 4, static_cast<int>(payloadSize))) {
      Logger::logError("Network", "Failed to receive packet payload");
      return std::nullopt;
    }
  }

  std::optional<Packet> packet = Serializer::deserialize(framed);
  if (!packet) {
    Logger::logError("Network", "Failed to deserialize packet");
    return std::nullopt;
  }

  return packet;
}

// check if the network is connected
bool Network::isConnected() const { return connected; }