/**
 * Socket I/O functions header
 *
 * @date 11-09-2026
 */

#pragma once
#include "utils/models/packet.h"
#include <cstdint>
#include <optional>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

namespace socket_io {

// create, bind, and listen to a socket
SOCKET listenTo(std::uint16_t port);

// receive exact number of bytes
bool recvExact(SOCKET socket, char *buffer, int bytes);

// send exact number of bytes
bool sendExact(SOCKET socket, const char *buffer, int bytes);

// read a packet from the socket
std::optional<Packet> readPacket(SOCKET socket);

// write a packet to the socket
bool writePacket(SOCKET socket, const Packet &packet);

} // namespace socket_io