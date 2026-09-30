/**
 * WebConnection tests
 *
 * @brief Accept key, upgrade, frame codec, and one browser session on ConnectionManager.
 * @date 29-09-2026
 */

#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/web_connection.h"
#include "utils/models/packet.h"
#include "utils/serializer.h"
#include "utils/socket_io.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef ERROR
#undef ERROR
#endif

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

void setRecvTimeout(SOCKET socket, DWORD milliseconds) {
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&milliseconds),
             sizeof(milliseconds));
}

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / "dcs-web-connection-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / ("node-" + std::to_string(rd()) + ".db");
    config::DB_PATH = path.string();
    config::NODE_ID = "web-connection-test";
    return &DatabaseManager::getInstance();
  }();
  return *instance;
}

const char *kSampleKey = "dGhlIHNhbXBsZSBub25jZQ==";
const char *kSampleAccept = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";

std::string sampleUpgrade() {
  return std::string("GET / HTTP/1.1\r\n"
                     "Host: 127.0.0.1\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: ") +
         kSampleKey +
         "\r\n"
         "Sec-WebSocket-Version: 13\r\n"
         "\r\n";
}

} // namespace

TEST_CASE("WebConnection accept key matches RFC 6455", "[web_connection][flow]") {
  REQUIRE(web_connection::acceptValue(kSampleKey) == kSampleAccept);
}

TEST_CASE("WebConnection upgrade response", "[web_connection][flow]") {
  const std::string response = web_connection::upgradeResponse(sampleUpgrade());
  REQUIRE(response.find("HTTP/1.1 101") == 0);
  REQUIRE(response.find(kSampleAccept) != std::string::npos);
}

TEST_CASE("WebConnection upgrade rejects a request without a key", "[web_connection][edge]") {
  const std::string request = "GET / HTTP/1.1\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Version: 13\r\n"
                              "\r\n";
  REQUIRE(web_connection::upgradeResponse(request).empty());
}

TEST_CASE("WebConnection binary frame round-trip", "[web_connection][flow]") {
  const std::string plain = web_connection::encodeFrame(web_connection::WsOpcode::Binary, "hi", false);
  const web_connection::FrameDecode decoded = web_connection::decodeFrame(plain);
  REQUIRE(decoded.status == web_connection::FrameDecode::Status::Ok);
  REQUIRE(decoded.opcode == web_connection::WsOpcode::Binary);
  REQUIRE(decoded.payload == "hi");

  const std::string masked =
      web_connection::encodeFrame(web_connection::WsOpcode::Binary, "hi", true, 0x01020304);
  const web_connection::FrameDecode unmasked = web_connection::decodeFrame(masked);
  REQUIRE(unmasked.status == web_connection::FrameDecode::Status::Ok);
  REQUIRE(unmasked.payload == "hi");

  REQUIRE(web_connection::decodeFrame(plain.substr(0, 1)).status ==
          web_connection::FrameDecode::Status::Incomplete);

  std::string illegal = plain;
  illegal[0] = static_cast<char>(static_cast<unsigned char>(illegal[0]) | 0x40);
  REQUIRE(web_connection::decodeFrame(illegal).status == web_connection::FrameDecode::Status::Error);
}

TEST_CASE("WebConnection session receives welcome and heartbeat pong", "[web_connection][flow]") {
  REQUIRE(winsock().ok);
  (void)db();

  ConnectionManager connections;
  REQUIRE(connections.startListening(0));
  web_connection::WebConnection web(connections);
  REQUIRE(web.startListening(0));
  REQUIRE(web.port() != 0);
  std::thread accept([&] { web.acceptLoop(); });

  const SOCKET browser = socket_io::connectTo("127.0.0.1", web.port());
  REQUIRE(browser != INVALID_SOCKET);
  setRecvTimeout(browser, 5000);
  const std::string upgrade = sampleUpgrade();
  REQUIRE(socket_io::sendExact(browser, upgrade.data(), static_cast<int>(upgrade.size())));

  std::string pending;
  char block[4096];
  while (pending.find("\r\n\r\n") == std::string::npos) {
    const int got = recv(browser, block, sizeof(block), 0);
    REQUIRE(got > 0);
    pending.append(block, static_cast<std::size_t>(got));
  }
  const std::size_t headerEnd = pending.find("\r\n\r\n") + 4;
  REQUIRE(pending.find("101") != std::string::npos);
  pending.erase(0, headerEnd);

  const auto readFrame = [&]() {
    const auto ensure = [&](std::size_t count) {
      while (pending.size() < count) {
        const int got = recv(browser, block, sizeof(block), 0);
        if (got <= 0) {
          return false;
        }
        pending.append(block, static_cast<std::size_t>(got));
      }
      return true;
    };
    if (!ensure(2)) {
      return std::string{};
    }
    const auto code = static_cast<unsigned char>(pending[1]) & 0x7F;
    std::uint64_t payloadLength = code;
    std::size_t header = 2;
    if (code == 126) {
      header = 4;
      if (!ensure(header)) {
        return std::string{};
      }
      payloadLength = (static_cast<std::uint64_t>(static_cast<unsigned char>(pending[2])) << 8) |
                      static_cast<unsigned char>(pending[3]);
    } else if (code == 127) {
      header = 10;
      if (!ensure(header)) {
        return std::string{};
      }
      payloadLength = 0;
      for (int i = 0; i < 8; ++i) {
        payloadLength = (payloadLength << 8) | static_cast<unsigned char>(pending[2 + i]);
      }
    }
    if (!ensure(header + static_cast<std::size_t>(payloadLength))) {
      return std::string{};
    }
    const std::size_t total = header + static_cast<std::size_t>(payloadLength);
    std::string frame = pending.substr(0, total);
    pending.erase(0, total);
    return frame;
  };

  const web_connection::FrameDecode welcomeFrame = web_connection::decodeFrame(readFrame());
  REQUIRE(welcomeFrame.status == web_connection::FrameDecode::Status::Ok);
  const std::optional<Packet> welcome = Serializer::deserialize(welcomeFrame.payload);
  REQUIRE(welcome.has_value());
  REQUIRE(welcome->type == Packet::PacketType::ROOM_LIST);

  Packet ping("client", "server", Packet::PacketType::HEARTBEAT, "", "ping");
  ping.timestamp = 1;
  const std::string framed = Serializer::serialize(ping);
  const std::string wire =
      web_connection::encodeFrame(web_connection::WsOpcode::Binary, framed, true, 0x0A0B0C0D);
  REQUIRE(socket_io::sendExact(browser, wire.data(), static_cast<int>(wire.size())));

  const web_connection::FrameDecode pongFrame = web_connection::decodeFrame(readFrame());
  REQUIRE(pongFrame.status == web_connection::FrameDecode::Status::Ok);
  const std::optional<Packet> pong = Serializer::deserialize(pongFrame.payload);
  REQUIRE(pong.has_value());
  REQUIRE(pong->type == Packet::PacketType::HEARTBEAT);
  REQUIRE(pong->message == "pong");

  socket_io::close(browser);
  web.stopListening();
  accept.join();
  connections.stopListening();
  web.joinClientThreads();
}
