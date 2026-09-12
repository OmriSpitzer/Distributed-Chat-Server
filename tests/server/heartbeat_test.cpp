/**
 * Heartbeat unit tests
 *
 * @brief Includes: construct / destroy without start, stop without start, start then stop,
 * double start / double stop, restart, stop interrupts wait, no ping before interval,
 * destructor joins a running worker, concurrent start / stop, empty snapshot log,
 * live client receives ping, closed session is not pinged, silent client is dropped,
 * multiple clients, many start/stop cycles, typical keepalive flow.
 * @date 12-09-2026
 */

#include "config/config.h"
#include "server/connection_manager.h"
#include "server/heartbeat.h"
#include "utils/models/log_message.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/socket_io.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * 1. destructor without start
 * 2. stop without start
 * 3. start then stop
 * 4. double start
 * 5. double stop
 * 6. restart after stop
 * 7. stop interrupts wait
 * 8. no ping before the first interval
 * 9. destructor joins a running worker
 * 10. concurrent start
 * 11. concurrent stop
 * 12. concurrent start / stop
 * 13. many start / stop cycles
 * 14. empty snapshot logs total: 0
 * 15. live client receives ping
 * 16. closed session is not pinged
 * 17. silent client is dropped after timeout
 * 18. multiple clients each receive ping
 * 19. typical keepalive flow
 */

namespace {

constexpr const char *kSrc = "Heartbeat";
constexpr auto kPingWait =
    std::chrono::milliseconds(config::HEARTBEAT_INTERVAL + 2000);
constexpr auto kTimeoutWait =
    std::chrono::milliseconds(config::HEARTBEAT_TIMEOUT + 500);

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

bool hasHeartbeatContaining(std::string_view needle) {
  const Logger &logger = Logger::getInstance();
  const std::size_t size = Logger::size();
  for (std::size_t i = 0; i < size; ++i) {
    const LogMessage msg = logger.getMessage(i);
    if (msg.getSource() == kSrc && msg.getType() == LogMessage::Type::HEARTBEAT &&
        msg.getMessage().find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool setRecvTimeout(SOCKET socket, DWORD milliseconds) {
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&milliseconds),
                    sizeof(milliseconds)) == 0;
}

struct Fixture {
  ConnectionManager connections;
  Heartbeat heartbeat;
  std::thread acceptThread;
  std::vector<SOCKET> clients;

  Fixture() : heartbeat(connections) {}

  Fixture(const Fixture &) = delete;
  Fixture &operator=(const Fixture &) = delete;

  ~Fixture() {
    heartbeat.stop();
    for (SOCKET socket : clients) {
      if (socket != INVALID_SOCKET) {
        closesocket(socket);
      }
    }
    connections.stopListening();
    if (acceptThread.joinable()) {
      acceptThread.join();
    }
  }

  bool listen() {
    if (!winsock().ok) {
      return false;
    }
    if (!connections.startListening(0)) {
      return false;
    }
    acceptThread = std::thread([this] { connections.acceptLoop(); });
    return true;
  }

  SOCKET connectClient() {
    const SOCKET listenFd = static_cast<SOCKET>(connections.getListeningSocket());
    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    if (getsockname(listenFd, reinterpret_cast<sockaddr *>(&bound), &boundLen) != 0) {
      return INVALID_SOCKET;
    }

    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET) {
      return INVALID_SOCKET;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = bound.sin_port;
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(client, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) != 0) {
      closesocket(client);
      return INVALID_SOCKET;
    }

    clients.push_back(client);
    const std::size_t expected = clients.size();
    if (!waitUntil(std::chrono::seconds(2),
                   [&] { return connections.getSessions().size() >= expected; })) {
      return INVALID_SOCKET;
    }
    return client;
  }
};

} // namespace

// 1. destructor without start
TEST_CASE("Heartbeat destructor without start does not log Stopped", "[heartbeat][dtor]") {
  resetLogger();

  {
    ConnectionManager connections;
    Heartbeat heartbeat(connections);
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 0);
}

// 2. stop without start
TEST_CASE("Heartbeat stop without start is a no-op", "[heartbeat][stop]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.stop();
  heartbeat.stop();

  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == 0);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 0);
}

// 3. start then stop
TEST_CASE("Heartbeat start then stop logs Started and Stopped", "[heartbeat][start][stop]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.start();
  REQUIRE(waitUntil(std::chrono::seconds(1),
                    [&] { return countLogs(LogMessage::Type::INFO, "Started") == 1; }));

  heartbeat.stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == 1);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 4. double start
TEST_CASE("Heartbeat double start logs Started once", "[heartbeat][start][edge]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.start();
  heartbeat.start();
  heartbeat.start();

  REQUIRE(waitUntil(std::chrono::seconds(1),
                    [&] { return countLogs(LogMessage::Type::INFO, "Started") == 1; }));
  heartbeat.stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == 1);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 5. double stop
TEST_CASE("Heartbeat double stop logs Stopped once", "[heartbeat][stop][edge]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.start();
  REQUIRE(waitUntil(std::chrono::seconds(1),
                    [&] { return countLogs(LogMessage::Type::INFO, "Started") >= 1; }));

  heartbeat.stop();
  heartbeat.stop();
  heartbeat.stop();

  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 6. restart after stop
TEST_CASE("Heartbeat can restart after stop", "[heartbeat][restart]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);

  heartbeat.start();
  heartbeat.stop();
  heartbeat.start();
  heartbeat.stop();

  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == 2);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 2);
}

// 7. stop interrupts wait
TEST_CASE("Heartbeat stop returns before the next interval", "[heartbeat][stop][edge]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.start();

  const auto begin = std::chrono::steady_clock::now();
  heartbeat.stop();
  const auto elapsed = std::chrono::steady_clock::now() - begin;

  REQUIRE(elapsed < std::chrono::milliseconds(config::HEARTBEAT_INTERVAL));
  REQUIRE(elapsed < std::chrono::seconds(1));
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 8. no ping before the first interval
TEST_CASE("Heartbeat does not ping before the first interval", "[heartbeat][ping][edge]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  heartbeat.stop();

  REQUIRE(countLogs(LogMessage::Type::HEARTBEAT) == 0);
}

// 9. destructor joins a running worker
TEST_CASE("Heartbeat destructor stops a running worker", "[heartbeat][dtor]") {
  resetLogger();

  {
    ConnectionManager connections;
    Heartbeat heartbeat(connections);
    heartbeat.start();
    REQUIRE(waitUntil(std::chrono::seconds(1),
                      [&] { return countLogs(LogMessage::Type::INFO, "Started") == 1; }));
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 10. concurrent start
TEST_CASE("Heartbeat concurrent start starts once", "[heartbeat][thread][start]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);

  std::vector<std::thread> threads;
  threads.reserve(8);
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&] { heartbeat.start(); });
  }
  for (std::thread &thread : threads) {
    thread.join();
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == 1);
  heartbeat.stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 11. concurrent stop
TEST_CASE("Heartbeat concurrent stop stops once", "[heartbeat][thread][stop]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.start();
  REQUIRE(waitUntil(std::chrono::seconds(1),
                    [&] { return countLogs(LogMessage::Type::INFO, "Started") == 1; }));

  std::vector<std::thread> threads;
  threads.reserve(8);
  for (int i = 0; i < 8; ++i) {
    threads.emplace_back([&] { heartbeat.stop(); });
  }
  for (std::thread &thread : threads) {
    thread.join();
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 12. concurrent start / stop
TEST_CASE("Heartbeat concurrent start and stop do not deadlock", "[heartbeat][thread]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  std::atomic<bool> run{true};

  std::vector<std::thread> threads;
  threads.reserve(4);
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([&] {
      while (run.load()) {
        heartbeat.start();
        heartbeat.stop();
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  run = false;
  for (std::thread &thread : threads) {
    thread.join();
  }
  heartbeat.stop();
  SUCCEED();
}

// 13. many start / stop cycles
TEST_CASE("Heartbeat many start stop cycles stay consistent", "[heartbeat][restart][edge]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  constexpr int kCycles = 20;

  for (int i = 0; i < kCycles; ++i) {
    heartbeat.start();
    heartbeat.stop();
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == kCycles);
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == kCycles);
}

// 14. empty snapshot logs total: 0
TEST_CASE("Heartbeat empty snapshot logs total 0", "[heartbeat][ping][slow]") {
  resetLogger();

  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  heartbeat.start();

  REQUIRE(waitUntil(kPingWait, [&] { return hasHeartbeatContaining("total: 0"); }));
  heartbeat.stop();
}

// 15. live client receives ping
TEST_CASE("Heartbeat live client receives ping", "[heartbeat][ping][slow]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  REQUIRE(fixture.listen());
  const SOCKET client = fixture.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, static_cast<DWORD>(kPingWait.count())));

  fixture.heartbeat.start();
  const std::optional<Packet> packet = socket_io::readPacket(client);
  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::HEARTBEAT);
  REQUIRE(packet->message == "ping");
  REQUIRE(packet->sender == "server");
}

// 16. closed session is not pinged
TEST_CASE("Heartbeat skips closed sessions", "[heartbeat][ping][closed][slow]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  REQUIRE(fixture.listen());
  const SOCKET client = fixture.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, static_cast<DWORD>(kPingWait.count())));

  auto sessions = fixture.connections.getSessions();
  REQUIRE(sessions.size() == 1);
  sessions.begin()->second->markClosed();

  fixture.heartbeat.start();
  const std::optional<Packet> packet = socket_io::readPacket(client);
  REQUIRE_FALSE(packet.has_value());
}

// 17. silent client is dropped after timeout
TEST_CASE("Heartbeat closes a silent client after timeout", "[heartbeat][timeout][slow]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  REQUIRE(fixture.listen());
  REQUIRE(fixture.connectClient() != INVALID_SOCKET);

  std::this_thread::sleep_for(kTimeoutWait);
  {
    auto sessions = fixture.connections.getSessions();
    REQUIRE_FALSE(sessions.empty());
    REQUIRE_FALSE(sessions.begin()->second->isAlive());
  }

  fixture.heartbeat.start();
  REQUIRE(waitUntil(kPingWait, [&] {
    auto sessions = fixture.connections.getSessions();
    if (sessions.empty()) {
      return true;
    }
    return sessions.begin()->second->isClosed();
  }));
}

// 18. multiple clients each receive ping
TEST_CASE("Heartbeat pings every live client", "[heartbeat][ping][slow]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  REQUIRE(fixture.listen());
  const SOCKET first = fixture.connectClient();
  const SOCKET second = fixture.connectClient();
  REQUIRE(first != INVALID_SOCKET);
  REQUIRE(second != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(first, static_cast<DWORD>(kPingWait.count())));
  REQUIRE(setRecvTimeout(second, static_cast<DWORD>(kPingWait.count())));

  fixture.heartbeat.start();

  const std::optional<Packet> pingFirst = socket_io::readPacket(first);
  const std::optional<Packet> pingSecond = socket_io::readPacket(second);
  REQUIRE(pingFirst.has_value());
  REQUIRE(pingSecond.has_value());
  REQUIRE(pingFirst->type == Packet::PacketType::HEARTBEAT);
  REQUIRE(pingSecond->type == Packet::PacketType::HEARTBEAT);
  REQUIRE(pingFirst->message == "ping");
  REQUIRE(pingSecond->message == "ping");

  REQUIRE(waitUntil(kPingWait, [&] { return hasHeartbeatContaining("total: 2"); }));
}

// 19. typical keepalive flow
TEST_CASE("Heartbeat typical start ping stop flow", "[heartbeat][flow][slow]") {
  resetLogger();
  REQUIRE(winsock().ok);

  Fixture fixture;
  REQUIRE(fixture.listen());
  const SOCKET client = fixture.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, static_cast<DWORD>(kPingWait.count())));

  fixture.heartbeat.start();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Started") == 1);

  const std::optional<Packet> packet = socket_io::readPacket(client);
  REQUIRE(packet.has_value());
  REQUIRE(packet->type == Packet::PacketType::HEARTBEAT);

  fixture.heartbeat.stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
  REQUIRE(hasHeartbeatContaining("total:"));
}
