/**
 * Socket I/O unit tests
 *
 * @brief Includes: listenTo, sendExact / recvExact edges, writePacket frames
 * matching Serializer, readPacket from Serializer frames, invalid / truncated /
 * oversize frames, peer close, back-to-back packets.
 * @date 12-09-2026
 */

#include "utils/models/packet.h"
#include "utils/serializer.h"
#include "utils/socket_io.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * 1. listenTo binds and accepts
 * 2. sendExact / recvExact edges
 * 3. writePacket frames match Serializer::serialize
 * 4. readPacket restores Serializer frames
 * 5. writePacket / readPacket round-trip edges
 * 6. writePacket rejects packets Serializer cannot serialize
 * 7. readPacket rejects empty / truncated / oversize frames
 * 8. peer close and invalid sockets
 * 9. back-to-back packets
 */

namespace {

// global Winsock — started once for the process
struct Winsock {
  Winsock() {
    WSADATA data;
    ok = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
  }
  ~Winsock() {
    if (ok)
      WSACleanup();
  }
  bool ok{false};
};

Winsock &winsock() {
  static Winsock instance;
  return instance;
}

// loopback pair: client <-> accepted server socket
struct ConnectedPair {
  SOCKET listener{INVALID_SOCKET};
  SOCKET server{INVALID_SOCKET};
  SOCKET client{INVALID_SOCKET};

  ConnectedPair() = default;
  ConnectedPair(const ConnectedPair &) = delete;
  ConnectedPair &operator=(const ConnectedPair &) = delete;

  ~ConnectedPair() {
    if (client != INVALID_SOCKET)
      closesocket(client);
    if (server != INVALID_SOCKET)
      closesocket(server);
    if (listener != INVALID_SOCKET)
      closesocket(listener);
  }

  bool open() {
    listener = socket_io::listenTo(0);
    if (listener == INVALID_SOCKET)
      return false;

    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    if (getsockname(listener, reinterpret_cast<sockaddr *>(&bound), &boundLen) != 0)
      return false;

    client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET)
      return false;

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = bound.sin_port;
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(client, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) != 0)
      return false;

    server = accept(listener, nullptr, nullptr);
    return server != INVALID_SOCKET;
  }
};

// check if two packets are equal
void requirePacketsEqual(const Packet &expected, const Packet &actual) {
  REQUIRE(actual.type == expected.type);
  REQUIRE(actual.sender == expected.sender);
  REQUIRE(actual.receiver == expected.receiver);
  REQUIRE(actual.room == expected.room);
  REQUIRE(actual.message == expected.message);
  REQUIRE(actual.timestamp == expected.timestamp);
  REQUIRE(actual.responseCode == expected.responseCode);
}

// make a packet
Packet makePacket(Packet::PacketType type, const std::string &sender = "alice",
                  const std::string &receiver = "bob", const std::string &room = "general",
                  const std::string &message = "hello", std::uint64_t timestamp = 1,
                  int responseCode = 0) {
  Packet packet(sender, receiver, type, room, message, responseCode);
  packet.timestamp = timestamp;
  return packet;
}

// put a 32-bit integer in big-endian format
void putBe32(char out[4], std::uint32_t n) {
  out[0] = static_cast<char>((n >> 24) & 0xFF);
  out[1] = static_cast<char>((n >> 16) & 0xFF);
  out[2] = static_cast<char>((n >> 8) & 0xFF);
  out[3] = static_cast<char>(n & 0xFF);
}

} // namespace

// 1. listenTo binds and accepts
TEST_CASE("socket_io listenTo binds and accepts", "[socket_io][listen][edge]") {
  REQUIRE(winsock().ok);

  SECTION("port 0 succeeds") {
    SOCKET listener = socket_io::listenTo(0);
    REQUIRE(listener != INVALID_SOCKET);
    closesocket(listener);
  }

  SECTION("accepted connection is a distinct socket") {
    ConnectedPair pair;
    REQUIRE(pair.open());
    REQUIRE(pair.server != pair.client);
    REQUIRE(pair.server != pair.listener);
  }
}

// 2. sendExact / recvExact edges
TEST_CASE("socket_io sendExact / recvExact edges", "[socket_io][exact][edge]") {
  REQUIRE(winsock().ok);
  ConnectedPair pair;
  REQUIRE(pair.open());

  SECTION("zero bytes succeeds without I/O") {
    char dummy = 0;
    REQUIRE(socket_io::sendExact(pair.client, &dummy, 0));
    REQUIRE(socket_io::recvExact(pair.server, &dummy, 0));
  }

  SECTION("exact buffer round-trip") {
    const char msg[] = "abcd";
    REQUIRE(socket_io::sendExact(pair.client, msg, 4));
    char buf[4]{};
    REQUIRE(socket_io::recvExact(pair.server, buf, 4));
    REQUIRE(std::string(buf, 4) == "abcd");
  }

  SECTION("recvExact waits for the full count") {
    REQUIRE(socket_io::sendExact(pair.client, "ab", 2));
    REQUIRE(socket_io::sendExact(pair.client, "cd", 2));
    char buf[4]{};
    REQUIRE(socket_io::recvExact(pair.server, buf, 4));
    REQUIRE(std::string(buf, 4) == "abcd");
  }

  SECTION("embedded nulls") {
    const char msg[] = {'a', '\0', 'b', '\0'};
    REQUIRE(socket_io::sendExact(pair.client, msg, 4));
    char buf[4]{};
    REQUIRE(socket_io::recvExact(pair.server, buf, 4));
    REQUIRE(std::string(buf, 4) == std::string(msg, 4));
  }

  SECTION("invalid socket") {
    char buf[1]{};
    REQUIRE_FALSE(socket_io::sendExact(INVALID_SOCKET, buf, 1));
    REQUIRE_FALSE(socket_io::recvExact(INVALID_SOCKET, buf, 1));
  }

  SECTION("peer closed before recvExact finishes") {
    REQUIRE(socket_io::sendExact(pair.client, "xy", 2));
    closesocket(pair.client);
    pair.client = INVALID_SOCKET;
    char buf[4]{};
    REQUIRE_FALSE(socket_io::recvExact(pair.server, buf, 4));
  }
}

// 3. writePacket frames match Serializer::serialize
TEST_CASE("socket_io writePacket frames match Serializer", "[socket_io][write][serializer][edge]") {
  REQUIRE(winsock().ok);
  ConnectedPair pair;
  REQUIRE(pair.open());

  const Packet packet =
      makePacket(Packet::PacketType::MESSAGE, "alice", "bob", "general", "hello", 42, 200);
  const std::string framed = Serializer::serialize(packet);
  REQUIRE_FALSE(framed.empty());

  REQUIRE(socket_io::writePacket(pair.client, packet));
  std::string got(framed.size(), '\0');
  REQUIRE(socket_io::recvExact(pair.server, got.data(), static_cast<int>(framed.size())));
  REQUIRE(got == framed);

  const auto restored = Serializer::deserialize(got);
  REQUIRE(restored.has_value());
  requirePacketsEqual(packet, *restored);
}

// 4. readPacket restores Serializer frames
TEST_CASE("socket_io readPacket restores Serializer frames",
          "[socket_io][read][serializer][edge]") {
  REQUIRE(winsock().ok);
  ConnectedPair pair;
  REQUIRE(pair.open());

  const Packet packet = makePacket(Packet::PacketType::GOSSIP_EVENT, "node-a", "*", "Lobby",
                                   "MESSAGE|id|u|hi|1", 7, 0);
  const std::string framed = Serializer::serialize(packet);
  REQUIRE_FALSE(framed.empty());

  REQUIRE(socket_io::sendExact(pair.client, framed.data(), static_cast<int>(framed.size())));
  const auto restored = socket_io::readPacket(pair.server);
  REQUIRE(restored.has_value());
  requirePacketsEqual(packet, *restored);
}

// 5. writePacket / readPacket round-trip edges
TEST_CASE("socket_io writePacket / readPacket round-trip edges", "[socket_io][roundtrip][edge]") {
  REQUIRE(winsock().ok);
  ConnectedPair pair;
  REQUIRE(pair.open());

  SECTION("empty fields") {
    const Packet packet = makePacket(Packet::PacketType::MESSAGE, "", "", "", "", 0, 0);
    REQUIRE(socket_io::writePacket(pair.client, packet));
    const auto restored = socket_io::readPacket(pair.server);
    REQUIRE(restored.has_value());
    requirePacketsEqual(packet, *restored);
  }

  SECTION("unicode") {
    const Packet packet =
        makePacket(Packet::PacketType::MESSAGE, "עֹמְרִי", "ボブ", "חדר", "שלום 👋", 999, 1);
    REQUIRE(socket_io::writePacket(pair.client, packet));
    const auto restored = socket_io::readPacket(pair.server);
    REQUIRE(restored.has_value());
    requirePacketsEqual(packet, *restored);
  }

  SECTION("whitespace and pipes") {
    const Packet packet =
        makePacket(Packet::PacketType::MESSAGE, "  a  ", "{b}", "r|oom", "msg|with|pipes", 11, 0);
    REQUIRE(socket_io::writePacket(pair.client, packet));
    const auto restored = socket_io::readPacket(pair.server);
    REQUIRE(restored.has_value());
    requirePacketsEqual(packet, *restored);
  }

  SECTION("numeric extremes") {
    const Packet packet =
        makePacket(Packet::PacketType::LOGIN, "a", "s", "", "", UINT64_MAX, INT32_MIN);
    REQUIRE(socket_io::writePacket(pair.client, packet));
    const auto restored = socket_io::readPacket(pair.server);
    REQUIRE(restored.has_value());
    requirePacketsEqual(packet, *restored);
  }

  SECTION("all packet types") {
    const std::vector<Packet::PacketType> types = {
        Packet::PacketType::DEFAULT,       Packet::PacketType::LOGIN,
        Packet::PacketType::LOGOUT,        Packet::PacketType::MESSAGE,
        Packet::PacketType::ROOM_JOIN,     Packet::PacketType::ROOM_LEAVE,
        Packet::PacketType::HEARTBEAT,     Packet::PacketType::REGISTER,
        Packet::PacketType::GOSSIP_HELLO,  Packet::PacketType::GOSSIP_EVENT,
        Packet::PacketType::GOSSIP_DIGEST, Packet::PacketType::GOSSIP_PULL,
    };
    for (Packet::PacketType type : types) {
      ConnectedPair typed;
      REQUIRE(typed.open());
      const Packet packet = makePacket(type);
      REQUIRE(socket_io::writePacket(typed.client, packet));
      const auto restored = socket_io::readPacket(typed.server);
      REQUIRE(restored.has_value());
      requirePacketsEqual(packet, *restored);
    }
  }
}

// 6. writePacket rejects packets Serializer cannot serialize
TEST_CASE("socket_io writePacket rejects invalid packets", "[socket_io][write][edge]") {
  REQUIRE(winsock().ok);
  ConnectedPair pair;
  REQUIRE(pair.open());

  SECTION("out of range type") {
    Packet packet = makePacket(Packet::PacketType::MESSAGE);
    packet.type = static_cast<Packet::PacketType>(255);
    REQUIRE(Serializer::serialize(packet).empty());
    REQUIRE_FALSE(socket_io::writePacket(pair.client, packet));
  }

  SECTION("invalid write does not pollute the stream") {
    Packet bad = makePacket(Packet::PacketType::MESSAGE);
    bad.type = static_cast<Packet::PacketType>(255);
    REQUIRE_FALSE(socket_io::writePacket(pair.client, bad));

    const Packet good = makePacket(Packet::PacketType::HEARTBEAT, "c", "s", "", "ping", 1, 0);
    REQUIRE(socket_io::writePacket(pair.client, good));
    const auto restored = socket_io::readPacket(pair.server);
    REQUIRE(restored.has_value());
    requirePacketsEqual(good, *restored);
  }
}

// 7. readPacket rejects empty / truncated / oversize frames
TEST_CASE("socket_io readPacket rejects bad frames", "[socket_io][read][edge]") {
  REQUIRE(winsock().ok);
  ConnectedPair pair;
  REQUIRE(pair.open());

  SECTION("payload size zero") {
    char sizeBuf[4]{};
    REQUIRE(socket_io::sendExact(pair.client, sizeBuf, 4));
    REQUIRE_FALSE(socket_io::readPacket(pair.server).has_value());
  }

  SECTION("payload size over Serializer::MAX_PAYLOAD_BYTES") {
    char sizeBuf[4]{};
    putBe32(sizeBuf, Serializer::MAX_PAYLOAD_BYTES + 1);
    REQUIRE(socket_io::sendExact(pair.client, sizeBuf, 4));
    REQUIRE_FALSE(socket_io::readPacket(pair.server).has_value());
  }

  SECTION("truncated length prefix") {
    REQUIRE(socket_io::sendExact(pair.client, "\x00\x00", 2));
    closesocket(pair.client);
    pair.client = INVALID_SOCKET;
    REQUIRE_FALSE(socket_io::readPacket(pair.server).has_value());
  }

  SECTION("truncated payload") {
    const Packet packet = makePacket(Packet::PacketType::MESSAGE);
    const std::string framed = Serializer::serialize(packet);
    REQUIRE(framed.size() > 8);
    REQUIRE(socket_io::sendExact(pair.client, framed.data(), 8));
    closesocket(pair.client);
    pair.client = INVALID_SOCKET;
    REQUIRE_FALSE(socket_io::readPacket(pair.server).has_value());
  }

  SECTION("corrupted type byte after valid length") {
    Packet packet = makePacket(Packet::PacketType::MESSAGE);
    std::string framed = Serializer::serialize(packet);
    REQUIRE(framed.size() > 4);
    framed[4] = static_cast<char>(200);
    REQUIRE(socket_io::sendExact(pair.client, framed.data(), static_cast<int>(framed.size())));
    REQUIRE_FALSE(socket_io::readPacket(pair.server).has_value());
  }
}

// 8. peer close and invalid sockets
TEST_CASE("socket_io peer close and invalid sockets", "[socket_io][close][edge]") {
  REQUIRE(winsock().ok);

  SECTION("readPacket on INVALID_SOCKET") {
    REQUIRE_FALSE(socket_io::readPacket(INVALID_SOCKET).has_value());
  }

  SECTION("writePacket on INVALID_SOCKET") {
    const Packet packet = makePacket(Packet::PacketType::MESSAGE);
    REQUIRE_FALSE(Serializer::serialize(packet).empty());
    REQUIRE_FALSE(socket_io::writePacket(INVALID_SOCKET, packet));
  }

  SECTION("readPacket when peer closed with no data") {
    ConnectedPair pair;
    REQUIRE(pair.open());
    closesocket(pair.client);
    pair.client = INVALID_SOCKET;
    REQUIRE_FALSE(socket_io::readPacket(pair.server).has_value());
  }
}

// 9. back-to-back packets
TEST_CASE("socket_io back-to-back packets", "[socket_io][roundtrip][edge]") {
  REQUIRE(winsock().ok);
  ConnectedPair pair;
  REQUIRE(pair.open());

  const Packet first = makePacket(Packet::PacketType::LOGIN, "alice", "server", "", "secret", 1, 0);
  const Packet second =
      makePacket(Packet::PacketType::MESSAGE, "alice", "bob", "general", "hello", 2, 0);
  const Packet third = makePacket(Packet::PacketType::GOSSIP_HELLO, "node-a", "*", "", "", 3, 0);

  REQUIRE(socket_io::writePacket(pair.client, first));
  REQUIRE(socket_io::writePacket(pair.client, second));
  REQUIRE(socket_io::writePacket(pair.client, third));

  const auto a = socket_io::readPacket(pair.server);
  const auto b = socket_io::readPacket(pair.server);
  const auto c = socket_io::readPacket(pair.server);
  REQUIRE(a.has_value());
  REQUIRE(b.has_value());
  REQUIRE(c.has_value());
  requirePacketsEqual(first, *a);
  requirePacketsEqual(second, *b);
  requirePacketsEqual(third, *c);
}
