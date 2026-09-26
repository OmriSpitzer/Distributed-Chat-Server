/**
 * Socket I/O helpers header
 *
 * @date 13-09-2026
 */

#pragma once
#include "utils/models/packet.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

namespace socket_io {

// create, bind, and listen to a socket
SOCKET listenTo(std::uint16_t port);

// create a TCP socket and connect to host:port (INVALID_SOCKET on failure)
SOCKET connectTo(std::string_view host, std::uint16_t port);

// receive exact number of bytes
bool recvExact(SOCKET socket, char *buffer, int bytes);

// send exact number of bytes
bool sendExact(SOCKET socket, const char *buffer, int bytes);

// read a packet from the socket
std::optional<Packet> readPacket(SOCKET socket);

// write a packet to the socket
bool writePacket(SOCKET socket, const Packet &packet);

// accept a connection from a listening socket
SOCKET acceptFrom(SOCKET listeningSocket);

// close a socket
void close(SOCKET socket);
} // namespace socket_io
