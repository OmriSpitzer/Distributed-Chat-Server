/**
 * Network unit tests
 *
 * @brief Includes: connect / disconnect edges, send / receive round-trip,
 * heartbeat ping/pong, chat push not queued, peer close, send failure teardown,
 * reconnect after disconnect.
 * @date 13-09-2026
 */

#include "client/network.h"
#include "config/config.h"
#include "utils/models/packet.h"
#include "utils/socket_io.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * 1. default is not connected
 * 2. connect then disconnect
 * 3. double connect fails
 * 4. disconnect without connect is a no-op
 * 5. send while disconnected fails
 * 6. send / receive round-trip
 * 7. heartbeat ping receives pong
 * 8. chat push is not queued
 * 9. peer close ends receive
 * 10. send after peer close fails
 * 11. reconnect after disconnect
 */

namespace {

struct Winsock {
  Winsock() {
    WSADATA data;
    ok = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
  }
  ~Winsock() {
    if (ok) {
      WSACleanup();
    }
  }
  bool ok{false};
};

Winsock &winsock() {
  static Winsock instance;
  return instance;
}

struct ConfigGuard {
  std::string host = config::SERVER_HOST;
  std::uint16_t port = config::PORT;
  ~ConfigGuard() {
    config::SERVER_HOST = host;
    config::PORT = port;
  }
};

struct TestPeer {
  SOCKET listener{INVALID_SOCKET};
  SOCKET peer{INVALID_SOCKET};

  TestPeer() = default;
  TestPeer(const TestPeer &) = delete;
  TestPeer &operator=(const TestPeer &) = delete;

  ~TestPeer() {
    if (peer != INVALID_SOCKET) {
      socket_io::close(peer);
    }
    if (listener != INVALID_SOCKET) {
      socket_io::close(listener);
    }
  }

  bool listen() {
    listener = socket_io::listenTo(0);
    if (listener == INVALID_SOCKET) {
      return false;
    }

    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    if (getsockname(listener, reinterpret_cast<sockaddr *>(&bound), &boundLen) != 0) {
      return false;
    }

    config::SERVER_HOST = "127.0.0.1";
    config::PORT = ntohs(bound.sin_port);
    return true;
  }

  bool acceptOnce() {
    peer = socket_io::acceptFrom(listener);
    return peer != INVALID_SOCKET;
  }
};

bool connectToPeer(Network &network, TestPeer &server) {
  std::thread acceptor([&server] { (void)server.acceptOnce(); });
  const bool ok = network.connect();
  acceptor.join();
  return ok && server.peer != INVALID_SOCKET && network.isConnected();
}

Packet makePacket(Packet::PacketType type, const std::string &message = "hello",
                  int responseCode = 200) {
  Packet packet("server", "client", type, "Lobby", message, responseCode);
  packet.timestamp = 1;
  return packet;
}

} // namespace

// 1. default is not connected
TEST_CASE("Network default is not connected", "[network][ctor][edge]") {
  REQUIRE(winsock().ok);
  const Network network;
  REQUIRE_FALSE(network.isConnected());
}

// 2. connect then disconnect
TEST_CASE("Network connect then disconnect", "[network][connect]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Network network;
  REQUIRE(connectToPeer(network, server));
  network.disconnect();
  REQUIRE_FALSE(network.isConnected());
}

// 3. double connect fails
TEST_CASE("Network double connect fails", "[network][connect][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Network network;
  REQUIRE(connectToPeer(network, server));
  REQUIRE_FALSE(network.connect());
  REQUIRE(network.isConnected());
}

// 4. disconnect without connect is a no-op
TEST_CASE("Network disconnect without connect is a no-op", "[network][disconnect][edge]") {
  REQUIRE(winsock().ok);
  Network network;
  network.disconnect();
  REQUIRE_FALSE(network.isConnected());
}

// 5. send while disconnected fails
TEST_CASE("Network send while disconnected fails", "[network][send][edge]") {
  REQUIRE(winsock().ok);
  Network network;
  REQUIRE_FALSE(network.sendPacket(makePacket(Packet::PacketType::MESSAGE)));
}

// 6. send / receive round-trip
TEST_CASE("Network send and receive round-trip", "[network][send][receive]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Network network;
  REQUIRE(connectToPeer(network, server));

  const Packet outbound = makePacket(Packet::PacketType::LOGIN, "user(alice|a@b.com|USER)");
  REQUIRE(network.sendPacket(outbound));

  const auto fromClient = socket_io::readPacket(server.peer);
  REQUIRE(fromClient.has_value());
  REQUIRE(fromClient->type == Packet::PacketType::LOGIN);
  REQUIRE(fromClient->message == outbound.message);

  const Packet inbound = makePacket(Packet::PacketType::LOGIN, "user(alice|a@b.com|USER)");
  REQUIRE(socket_io::writePacket(server.peer, inbound));

  const auto received = network.receivePacket();
  REQUIRE(received.has_value());
  REQUIRE(received->type == Packet::PacketType::LOGIN);
  REQUIRE(received->message == inbound.message);
}

// 7. heartbeat ping receives pong
TEST_CASE("Network heartbeat ping receives pong", "[network][heartbeat]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Network network;
  REQUIRE(connectToPeer(network, server));

  Packet ping("server", "client", Packet::PacketType::HEARTBEAT, "", "ping");
  REQUIRE(socket_io::writePacket(server.peer, ping));

  const auto pong = socket_io::readPacket(server.peer);
  REQUIRE(pong.has_value());
  REQUIRE(pong->type == Packet::PacketType::HEARTBEAT);
  REQUIRE(pong->message == "pong");
}

// 8. chat push is not queued
TEST_CASE("Network chat push is not queued", "[network][receive][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Network network;
  REQUIRE(connectToPeer(network, server));

  Packet chat("alice", "client", Packet::PacketType::MESSAGE, "Lobby", "hi", 0);
  REQUIRE(socket_io::writePacket(server.peer, chat));

  Packet ack = makePacket(Packet::PacketType::MESSAGE, "ok", 200);
  REQUIRE(socket_io::writePacket(server.peer, ack));

  const auto received = network.receivePacket();
  REQUIRE(received.has_value());
  REQUIRE(received->type == Packet::PacketType::MESSAGE);
  REQUIRE(received->responseCode == 200);
  REQUIRE(received->message == "ok");
}

// 9. peer close ends receive
TEST_CASE("Network peer close ends receive", "[network][receive][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Network network;
  REQUIRE(connectToPeer(network, server));

  socket_io::close(server.peer);
  server.peer = INVALID_SOCKET;

  const auto received = network.receivePacket();
  REQUIRE_FALSE(received.has_value());
  REQUIRE_FALSE(network.isConnected());
}

// 10. send after peer close fails
TEST_CASE("Network send after peer close fails", "[network][send][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Network network;
  REQUIRE(connectToPeer(network, server));

  socket_io::close(server.peer);
  server.peer = INVALID_SOCKET;

  REQUIRE_FALSE(network.receivePacket().has_value());
  REQUIRE_FALSE(network.isConnected());
  REQUIRE_FALSE(network.sendPacket(makePacket(Packet::PacketType::MESSAGE)));
}

// 11. reconnect after disconnect
TEST_CASE("Network reconnect after disconnect", "[network][connect][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer first;
  REQUIRE(first.listen());

  Network network;
  REQUIRE(connectToPeer(network, first));
  network.disconnect();
  REQUIRE_FALSE(network.isConnected());

  TestPeer second;
  REQUIRE(second.listen());
  REQUIRE(connectToPeer(network, second));
  network.disconnect();
  REQUIRE_FALSE(network.isConnected());
}
