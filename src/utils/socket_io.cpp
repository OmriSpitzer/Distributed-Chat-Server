/**
 * Socket I/O functions
 *
 * @brief Socket I/O functions for reading and writing packets.
 * @date 11-09-2026
 *
 */

#include "utils/socket_io.h"
#include "utils/serializer.h"
#include <cstdint>

namespace socket_io {

// create, bind, and listen to a socket
SOCKET listenTo(std::uint16_t port) {
  // create a socket
  SOCKET socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketFd == INVALID_SOCKET) {
    return INVALID_SOCKET;
  }

  // set the socket to reuse address
  int reuse = 1;
  setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse),
             sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);

  // bind the socket to the address
  if (bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    closesocket(socketFd);
    return INVALID_SOCKET;
  }

  // listen for incoming connections
  if (listen(socketFd, SOMAXCONN) != 0) {
    closesocket(socketFd);
    return INVALID_SOCKET;
  }

  return socketFd;
}

// receive exact number of bytes
bool recvExact(SOCKET socket, char *buffer, int bytes) {
  int received = 0;
  while (received < bytes) {
    // number of bytes received
    int n = recv(socket, buffer + received, bytes - received, 0);

    if (n <= 0) {
      return false;
    }
    received += n;
  }
  return true;
}

// send exact number of bytes
bool sendExact(SOCKET socket, const char *buffer, int bytes) {
  int sent = 0;
  while (sent < bytes) {

    // number of bytes sent
    int n = send(socket, buffer + sent, bytes - sent, 0);
    if (n <= 0) {
      return false;
    }
    sent += n;
  }
  return true;
}

// read a packet from the socket
std::optional<Packet> readPacket(SOCKET socket) {
  char sizeBuf[4];

  // check if the size buffer is received
  if (!recvExact(socket, sizeBuf, 4)) {
    return std::nullopt;
  }

  // convert the size buffer to a 32-bit unsigned integer
  std::uint32_t payloadSize =
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[0])) << 24) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[1])) << 16) |
      (static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[2])) << 8) |
      static_cast<std::uint32_t>(static_cast<unsigned char>(sizeBuf[3]));

  // check if the payload size is valid
  if (payloadSize == 0 || payloadSize > Serializer::MAX_PAYLOAD_BYTES) {
    return std::nullopt;
  }

  // create the framed buffer
  std::string framed(4 + payloadSize, '\0');
  framed[0] = sizeBuf[0];
  framed[1] = sizeBuf[1];
  framed[2] = sizeBuf[2];
  framed[3] = sizeBuf[3];

  // check if the payload is received
  if (!recvExact(socket, framed.data() + 4, static_cast<int>(payloadSize))) {
    return std::nullopt;
  }

  // deserialize the packet
  return Serializer::deserialize(framed);
}

// write a packet to the socket
bool writePacket(SOCKET socket, const Packet &packet) {
  const std::string framed = Serializer::serialize(packet);
  if (framed.empty())
    return false;
  return sendExact(socket, framed.data(), static_cast<int>(framed.size()));
}

} // namespace socket_io