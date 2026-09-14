/**
 * Server unit tests
 *
 * @brief Includes: construct / destroy without start, stop without start, start then stop,
 * double start / double stop, restart, destructor joins a running server, dashboard output,
 * listen failure, client connect after start, typical start / dashboard / stop flow.
 * @date 13-09-2026
 */

#include "config/config.h"
#include "server/database_manager.h"
#include "server/server.h"
#include "utils/models/log_message.h"
#include "utils/models/logger.h"
#include "utils/socket_io.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef ERROR
#undef ERROR
#endif

/**
 * 1. destructor without start
 * 2. stop without start
 * 3. start then stop
 * 4. double start
 * 5. double stop
 * 6. restart after stop
 * 7. destructor stops a running server
 * 8. dashboard while stopped
 * 9. dashboard while running
 * 10. dashboard lists peers
 * 11. start fails when the client port is exclusive
 * 12. client can connect after start
 * 13. typical start / dashboard / stop flow
 */

namespace {

constexpr const char *kSrc = "Server";

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

void resetLogger() { Logger::clear(); }

std::string unique(std::string_view prefix) {
  static std::atomic<std::uint64_t> seq{0};
  const auto n = seq.fetch_add(1);
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::string(prefix) + "_" + std::to_string(n) + "_" + std::to_string(now);
}

std::uint16_t nextPort() {
  static std::atomic<std::uint16_t> port{24000};
  return port.fetch_add(1);
}

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / "dcs-server-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / ("node-" + unique("pid") + "-" + std::to_string(rd()) + ".db");
    config::DB_PATH = path.string();
    return &DatabaseManager::getInstance();
  }();
  return *instance;
}

template <typename Predicate>
bool waitUntil(std::chrono::milliseconds timeout, Predicate pred) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (pred()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
  } while (std::chrono::steady_clock::now() < deadline);
  return pred();
}

std::size_t countLogs(LogMessage::Type type, std::string_view body = {}) {
  std::size_t n = 0;
  const Logger &logger = Logger::getInstance();
  const std::size_t size = Logger::size();
  for (std::size_t i = 0; i < size; ++i) {
    const LogMessage msg = logger.getMessage(i);
    if (msg.getSource() != kSrc || msg.getType() != type) {
      continue;
    }
    if (!body.empty() && msg.getMessage() != body) {
      continue;
    }
    ++n;
  }
  return n;
}

bool hasLogContaining(LogMessage::Type type, std::string_view needle) {
  const Logger &logger = Logger::getInstance();
  const std::size_t size = Logger::size();
  for (std::size_t i = 0; i < size; ++i) {
    const LogMessage msg = logger.getMessage(i);
    if (msg.getSource() == kSrc && msg.getType() == type &&
        msg.getMessage().find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::string startedMessage(std::uint16_t port) {
  return "Started on port " + std::to_string(port) + " with " +
         std::to_string(config::THREAD_COUNT) + " worker threads";
}

std::string captureDashboard(Server &server) {
  std::ostringstream out;
  auto *previous = std::cout.rdbuf(out.rdbuf());
  server.dashboard();
  std::cout.rdbuf(previous);
  return out.str();
}

void applyConfig(std::uint16_t port, std::uint16_t peerPort, const std::string &nodeId) {
  config::PORT = port;
  config::PEER_PORT = peerPort;
  config::NODE_ID = nodeId;
  config::PEERS.clear();
}

SOCKET occupyPort(std::uint16_t port) {
  SOCKET socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socketFd == INVALID_SOCKET) {
    return INVALID_SOCKET;
  }

  BOOL exclusive = TRUE;
  if (setsockopt(socketFd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive),
                 sizeof(exclusive)) != 0) {
    socket_io::close(socketFd);
    return INVALID_SOCKET;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  if (bind(socketFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
    socket_io::close(socketFd);
    return INVALID_SOCKET;
  }
  if (listen(socketFd, 1) != 0) {
    socket_io::close(socketFd);
    return INVALID_SOCKET;
  }
  return socketFd;
}

SOCKET connectClient(std::uint16_t port) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  do {
    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET) {
      return INVALID_SOCKET;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(client, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) == 0) {
      return client;
    }
    socket_io::close(client);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
  } while (std::chrono::steady_clock::now() < deadline);
  return INVALID_SOCKET;
}

struct Fixture {
  std::uint16_t port;
  std::uint16_t peerPort;
  std::string nodeId;
  Server server;

  Fixture() : port(nextPort()), peerPort(nextPort()), nodeId(unique("node")) {
    (void)db();
    applyConfig(port, peerPort, nodeId);
  }

  Fixture(const Fixture &) = delete;
  Fixture &operator=(const Fixture &) = delete;
};

} // namespace

// 1. destructor without start
TEST_CASE("Server destructor without start does not start listening", "[server][dtor]") {
  resetLogger();
  REQUIRE(winsock().ok);
  (void)db();

  const std::uint16_t port = nextPort();
  applyConfig(port, nextPort(), unique("node"));

  {
    Server server;
    REQUIRE_FALSE(server.isAlive());
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, "Starting server") == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, startedMessage(port)) == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server is not running") == 1);
}

// 2. stop without start
TEST_CASE("Server stop without start is a no-op", "[server][stop]") {
  resetLogger();
  REQUIRE(winsock().ok);
  (void)db();

  applyConfig(nextPort(), nextPort(), unique("node"));
  Server server;
  REQUIRE_FALSE(server.isAlive());

  server.stop();
  server.stop();

  REQUIRE_FALSE(server.isAlive());
  REQUIRE(countLogs(LogMessage::Type::INFO, "Starting server") == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server is not running") == 2);
}

// 3. start then stop
TEST_CASE("Server start then stop logs Started and Stopped", "[server][start][stop]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  fixture.server.start();

  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));
  REQUIRE(countLogs(LogMessage::Type::INFO, "Starting server") == 1);
  REQUIRE(countLogs(LogMessage::Type::INFO, startedMessage(fixture.port)) == 1);

  fixture.server.stop();
  REQUIRE_FALSE(fixture.server.isAlive());
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 1);
}

// 4. double start
TEST_CASE("Server double start logs Started once", "[server][start][edge]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  fixture.server.start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));

  fixture.server.start();
  fixture.server.start();

  REQUIRE(fixture.server.isAlive());
  REQUIRE(countLogs(LogMessage::Type::INFO, "Starting server") == 1);
  REQUIRE(countLogs(LogMessage::Type::INFO, startedMessage(fixture.port)) == 1);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server is already running") == 2);

  fixture.server.stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 1);
}

// 5. double stop
TEST_CASE("Server double stop logs Stopped once", "[server][stop][edge]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  fixture.server.start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));

  fixture.server.stop();
  fixture.server.stop();
  fixture.server.stop();

  REQUIRE_FALSE(fixture.server.isAlive());
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 1);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server is not running") == 2);
}

// 6. restart after stop
TEST_CASE("Server can restart after stop", "[server][restart]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  fixture.server.start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));
  fixture.server.stop();
  REQUIRE_FALSE(fixture.server.isAlive());

  fixture.server.start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));
  fixture.server.stop();

  REQUIRE(countLogs(LogMessage::Type::INFO, "Starting server") == 2);
  REQUIRE(countLogs(LogMessage::Type::INFO, startedMessage(fixture.port)) == 2);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 2);
}

// 7. destructor stops a running server
TEST_CASE("Server destructor stops a running server", "[server][dtor]") {
  resetLogger();
  REQUIRE(winsock().ok);
  (void)db();

  const std::uint16_t port = nextPort();
  applyConfig(port, nextPort(), unique("node"));

  {
    Server server;
    server.start();
    REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return server.isAlive(); }));
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, startedMessage(port)) == 1);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 1);
}

// 8. dashboard while stopped
TEST_CASE("Server dashboard while stopped", "[server][dashboard]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  const std::string text = captureDashboard(fixture.server);

  REQUIRE(text.find("Server dashboard") != std::string::npos);
  REQUIRE(text.find("Node id: " + fixture.nodeId) != std::string::npos);
  REQUIRE(text.find("Port: " + std::to_string(fixture.port)) != std::string::npos);
  REQUIRE(text.find("Peer port: " + std::to_string(fixture.peerPort)) != std::string::npos);
  REQUIRE(text.find("Database path: " + config::DB_PATH) != std::string::npos);
  REQUIRE(text.find("Peers: (none)") != std::string::npos);
  REQUIRE(text.find("Listening: no") != std::string::npos);
}

// 9. dashboard while running
TEST_CASE("Server dashboard while running", "[server][dashboard]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  fixture.server.start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));

  const std::string text = captureDashboard(fixture.server);
  REQUIRE(text.find("Listening: yes") != std::string::npos);
  REQUIRE(text.find("Node id: " + fixture.nodeId) != std::string::npos);

  fixture.server.stop();
  const std::string stopped = captureDashboard(fixture.server);
  REQUIRE(stopped.find("Listening: no") != std::string::npos);
}

// 10. dashboard lists peers
TEST_CASE("Server dashboard lists peers", "[server][dashboard]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  config::PEERS = {"127.0.0.1:1", "127.0.0.1:2"};
  const std::string text = captureDashboard(fixture.server);

  REQUIRE(text.find("Peers: 127.0.0.1:1, 127.0.0.1:2") != std::string::npos);
  REQUIRE(text.find("(none)") == std::string::npos);
}

// 11. start fails when the client port is exclusive
TEST_CASE("Server start fails when the client port is exclusive", "[server][start][edge]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  const SOCKET held = occupyPort(fixture.port);
  REQUIRE(held != INVALID_SOCKET);

  fixture.server.start();
  REQUIRE_FALSE(fixture.server.isAlive());
  REQUIRE(countLogs(LogMessage::Type::INFO, "Starting server") == 1);
  REQUIRE(hasLogContaining(LogMessage::Type::ERROR, "Failed to start listening on port " +
                                                        std::to_string(fixture.port)));
  REQUIRE(countLogs(LogMessage::Type::INFO, startedMessage(fixture.port)) == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 0);

  socket_io::close(held);
}

// 12. client can connect after start
TEST_CASE("Server accept loop accepts a client", "[server][accept]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  fixture.server.start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));

  const SOCKET client = connectClient(fixture.port);
  REQUIRE(client != INVALID_SOCKET);
  socket_io::close(client);

  fixture.server.stop();
  REQUIRE_FALSE(fixture.server.isAlive());
}

// 13. typical start / dashboard / stop flow
TEST_CASE("Server typical start dashboard stop flow", "[server][flow]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  REQUIRE_FALSE(fixture.server.isAlive());

  fixture.server.start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return fixture.server.isAlive(); }));
  REQUIRE(countLogs(LogMessage::Type::INFO, startedMessage(fixture.port)) == 1);

  const SOCKET client = connectClient(fixture.port);
  REQUIRE(client != INVALID_SOCKET);

  const std::string text = captureDashboard(fixture.server);
  REQUIRE(text.find("Listening: yes") != std::string::npos);
  REQUIRE(text.find("Node id: " + fixture.nodeId) != std::string::npos);

  socket_io::close(client);
  fixture.server.stop();
  REQUIRE_FALSE(fixture.server.isAlive());
  REQUIRE(countLogs(LogMessage::Type::INFO, "Server stopped") == 1);
}
