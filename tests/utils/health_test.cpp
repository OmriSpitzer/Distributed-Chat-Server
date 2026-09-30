/**
 * Health adapters + HealthMonitor unit tests
 *
 * @brief Fake IHealthCheck rollup, Db/Heartbeat/Server/Client adapters Up/Down,
 * healthStatusToString helpers.
 * @date 24-09-2026
 */

#include "client/client.h"
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/heartbeat.h"
#include "server/server.h"
#include "utils/health/client_health_adapter.h"
#include "utils/health/db_health_adapter.h"
#include "utils/health/health_monitor.h"
#include "utils/health/heartbeat_health_adapter.h"
#include "utils/health/i_health_check.h"
#include "utils/health/server_health_adapter.h"
#include "utils/logger/logger.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include "utils/socket_io.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef ERROR
#undef ERROR
#endif

/**
 * 1. healthStatusToString
 * 2. HealthReport::statusToString
 * 3. HealthMonitor empty → Up
 * 4. HealthMonitor all Up
 * 5. HealthMonitor one Down → overall Down
 * 6. HealthMonitor Degraded without Down
 * 7. HealthMonitor Down wins over Degraded
 * 8. DbHealthAdapter Up (live DB ping)
 * 9. HeartbeatHealthAdapter Down when not started
 * 10. HeartbeatHealthAdapter Up after start
 * 11. ServerHealthAdapter Down when not started
 * 12. ServerHealthAdapter Up after start
 * 13. ClientHealthAdapter Down when not connected
 * 14. ClientHealthAdapter Up on a live connection, Down after stop
 * 15. Monitor rollup with real Server adapters
 * 16. Client::health rolls up ClientHealthAdapter before connect
 * 17. Live Client + Db rollup: client Down → overall Down, then Up
 */

namespace {

class FakeHealthCheck : public IHealthCheck {
public:
  explicit FakeHealthCheck(HealthReport report) : report_(std::move(report)) {}

  HealthReport check() override { return report_; }

private:
  HealthReport report_;
};

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

std::string unique(std::string_view prefix) {
  static std::atomic<std::uint64_t> seq{0};
  const auto n = seq.fetch_add(1);
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::string(prefix) + "_" + std::to_string(n) + "_" + std::to_string(now);
}

std::uint16_t nextPort() {
  static std::atomic<std::uint16_t> port{19000};
  return port.fetch_add(1);
}

void applyServerConfig(std::uint16_t port, std::uint16_t peerPort, const std::string &nodeId,
                       const std::string &dbPath) {
  config::PORT = port;
  config::PEER_PORT = peerPort;
  config::WS_PORT = 0;
  config::NODE_ID = nodeId;
  config::DB_PATH = dbPath;
  config::PEERS.clear();
}

std::string tempDbPath() {
  std::random_device rd;
  const auto dir = std::filesystem::temp_directory_path() / "dcs-health-tests";
  std::filesystem::create_directories(dir);
  return (dir / ("node-" + unique("db") + "-" + std::to_string(rd()) + ".db")).string();
}

DatabaseManager &dbForTests() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    config::DB_PATH = tempDbPath();
    return &DatabaseManager::getInstance();
  }();
  return *instance;
}

struct ConfigGuard {
  std::string host = config::SERVER_HOST;
  std::uint16_t port = config::PORT;
  bool testMode = config::TEST_MODE;
  std::vector<ServerPoint> neighbors = config::NEIGHBOR_SERVERS;

  ~ConfigGuard() {
    config::SERVER_HOST = host;
    config::PORT = port;
    config::TEST_MODE = testMode;
    config::NEIGHBOR_SERVERS = neighbors;
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
    config::NEIGHBOR_SERVERS.clear();
    config::TEST_MODE = true;
    return true;
  }

  bool acceptOnce() {
    peer = socket_io::acceptFrom(listener);
    return peer != INVALID_SOCKET;
  }
};

Packet connectWelcome() {
  const User anon = User::anonymousUser();
  const std::string directory = Room::serializeList({
      Room(1, "Lobby", Room::RoomType::LOBBY),
      Room(2, "General", Room::RoomType::OTHER),
  });
  return Packet("server", anon.serialize(), Packet::PacketType::ROOM_LIST, "Lobby", directory, 0);
}

} // namespace

// 1. healthStatusToString
TEST_CASE("healthStatusToString maps each status", "[health][status]") {
  REQUIRE(healthStatusToString(HealthStatus::Up) == "Up");
  REQUIRE(healthStatusToString(HealthStatus::Degraded) == "Degraded");
  REQUIRE(healthStatusToString(HealthStatus::Down) == "Down");
}

// 2. HealthReport::statusToString
TEST_CASE("HealthReport statusToString uses status field", "[health][report]") {
  HealthReport report{"x", HealthStatus::Degraded, "n"};
  REQUIRE(report.statusToString() == "Degraded");
}

// 3. HealthMonitor empty → Up
TEST_CASE("HealthMonitor empty check is Up", "[health][monitor][empty]") {
  HealthMonitor monitor;
  REQUIRE(monitor.checkAll().empty());
  const HealthReport overall = monitor.check();
  REQUIRE(overall.name == "cluster");
  REQUIRE(overall.status == HealthStatus::Up);
  REQUIRE(overall.detail.empty());
}

// 4. HealthMonitor all Up
TEST_CASE("HealthMonitor all Up stays Up", "[health][monitor][flow]") {
  HealthMonitor monitor;
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"a", HealthStatus::Up, "ok"}));
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"b", HealthStatus::Up, "ok"}));

  const auto reports = monitor.checkAll();
  REQUIRE(reports.size() == 2);
  REQUIRE(monitor.check().status == HealthStatus::Up);
  REQUIRE(monitor.check().detail.empty());
}

// 5. HealthMonitor one Down → overall Down
TEST_CASE("HealthMonitor one Down yields overall Down", "[health][monitor][edge]") {
  HealthMonitor monitor;
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"a", HealthStatus::Up, "ok"}));
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"b", HealthStatus::Down, "fail"}));

  const HealthReport overall = monitor.check();
  REQUIRE(overall.status == HealthStatus::Down);
  REQUIRE(overall.detail.find("b:Down") != std::string::npos);
}

// 6. HealthMonitor Degraded without Down
TEST_CASE("HealthMonitor Degraded without Down", "[health][monitor][edge]") {
  HealthMonitor monitor;
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"a", HealthStatus::Up, "ok"}));
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"b", HealthStatus::Degraded, "slow"}));

  REQUIRE(monitor.check().status == HealthStatus::Degraded);
}

// 7. HealthMonitor Down wins over Degraded
TEST_CASE("HealthMonitor Down wins over Degraded", "[health][monitor][edge]") {
  HealthMonitor monitor;
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"a", HealthStatus::Degraded, "slow"}));
  monitor.add(std::make_unique<FakeHealthCheck>(HealthReport{"b", HealthStatus::Down, "fail"}));

  const HealthReport overall = monitor.check();
  REQUIRE(overall.status == HealthStatus::Down);
  REQUIRE(overall.detail.find("b:Down") != std::string::npos);
}

// 8. DbHealthAdapter Up
TEST_CASE("DbHealthAdapter reports Up when ping succeeds", "[health][adapter][db]") {
  DatabaseManager &db = dbForTests();
  DbHealthAdapter adapter(db);
  const HealthReport report = adapter.check();
  REQUIRE(report.name == "database");
  REQUIRE(report.status == HealthStatus::Up);
  REQUIRE(report.detail == "ok");
}

// 9–10. HeartbeatHealthAdapter
TEST_CASE("HeartbeatHealthAdapter tracks isRunning", "[health][adapter][heartbeat]") {
  REQUIRE(winsock().ok);
  ConnectionManager connections;
  Heartbeat heartbeat(connections);
  HeartbeatHealthAdapter adapter(heartbeat);

  SECTION("Down when not started") {
    const HealthReport report = adapter.check();
    REQUIRE(report.name == "heartbeat");
    REQUIRE(report.status == HealthStatus::Down);
    REQUIRE(report.detail == "not running");
  }

  SECTION("Up after start") {
    heartbeat.start();
    const HealthReport report = adapter.check();
    REQUIRE(report.name == "heartbeat");
    REQUIRE(report.status == HealthStatus::Up);
    REQUIRE(report.detail == "running");
    heartbeat.stop();
    REQUIRE(adapter.check().status == HealthStatus::Down);
  }
}

// 11–12. ServerHealthAdapter
TEST_CASE("ServerHealthAdapter tracks isAlive", "[health][adapter][server]") {
  REQUIRE(winsock().ok);
  Logger::clear();

  const std::uint16_t port = nextPort();
  const std::uint16_t peerPort = nextPort();
  const std::string nodeId = unique("node");
  const std::string dbPath = tempDbPath();
  applyServerConfig(port, peerPort, nodeId, dbPath);

  Server server;
  ServerHealthAdapter adapter(server);

  SECTION("Down when not started") {
    const HealthReport report = adapter.check();
    REQUIRE(report.name == "server");
    REQUIRE(report.status == HealthStatus::Down);
    REQUIRE(report.detail == "not alive");
  }

  SECTION("Up after start") {
    server.start();
    REQUIRE(server.isAlive());
    const HealthReport report = adapter.check();
    REQUIRE(report.name == "server");
    REQUIRE(report.status == HealthStatus::Up);
    REQUIRE(report.detail == "alive");
    server.stop();
    REQUIRE(adapter.check().status == HealthStatus::Down);
  }
}

// 13. ClientHealthAdapter Down when not connected
TEST_CASE("ClientHealthAdapter reports Down when not connected", "[health][adapter][client]") {
  Client client;
  ClientHealthAdapter adapter(client);
  const HealthReport report = adapter.check();
  REQUIRE(report.name == "client");
  REQUIRE(report.status == HealthStatus::Down);
  REQUIRE(report.detail == "not connected");
  REQUIRE_FALSE(client.isAlive());
}

// 14. ClientHealthAdapter Up on a live socket, Down after stop
TEST_CASE("ClientHealthAdapter reports Up when connected", "[health][adapter][client]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Client client;
  ClientHealthAdapter adapter(client);
  REQUIRE(adapter.check().status == HealthStatus::Down);

  std::thread acceptor([&server] {
    REQUIRE(server.acceptOnce());
    REQUIRE(socket_io::writePacket(server.peer, connectWelcome()));
  });
  REQUIRE(client.start());
  acceptor.join();

  const HealthReport up = adapter.check();
  REQUIRE(up.name == "client");
  REQUIRE(up.status == HealthStatus::Up);
  REQUIRE(up.detail == "connected");
  REQUIRE(client.isAlive());

  client.stop();
  REQUIRE(adapter.check().status == HealthStatus::Down);
  REQUIRE(adapter.check().detail == "not connected");
  REQUIRE_FALSE(client.isAlive());
}

// 15. Monitor rollup with real Server adapters
TEST_CASE("HealthMonitor rollup with Server Heartbeat Db adapters",
          "[health][monitor][adapter][flow]") {
  REQUIRE(winsock().ok);
  Logger::clear();

  const std::uint16_t port = nextPort();
  const std::uint16_t peerPort = nextPort();
  applyServerConfig(port, peerPort, unique("node"), tempDbPath());

  Server server;
  ConnectionManager connections;
  Heartbeat heartbeat(connections);

  HealthMonitor monitor;
  monitor.add(std::make_unique<ServerHealthAdapter>(server));
  monitor.add(std::make_unique<HeartbeatHealthAdapter>(heartbeat));
  monitor.add(std::make_unique<DbHealthAdapter>(dbForTests()));

  // server + heartbeat down, db up → overall Down
  HealthReport before = monitor.check();
  REQUIRE(before.status == HealthStatus::Down);
  REQUIRE(before.detail.find("server:Down") != std::string::npos);
  REQUIRE(before.detail.find("heartbeat:Down") != std::string::npos);

  server.start();
  heartbeat.start();
  HealthReport after = monitor.check();
  REQUIRE(after.status == HealthStatus::Up);
  REQUIRE(after.detail.empty());

  heartbeat.stop();
  server.stop();
}

// 15. Client::health is what the dashboard subtitle reads
TEST_CASE("Client health monitor reports Down before connect", "[health][monitor][client]") {
  Client client;
  client.health().add(std::make_unique<ClientHealthAdapter>(client));
  const HealthReport report = client.health().check();
  REQUIRE(report.status == HealthStatus::Down);
  REQUIRE(report.detail.find("client:Down") != std::string::npos);
  REQUIRE_FALSE(client.isAlive());
}

// 17. Live Client + Db: one Down child keeps the monitor Down until the client connects
TEST_CASE("HealthMonitor rollup with live Client and Db adapters",
          "[health][monitor][adapter][client]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Client client;
  HealthMonitor monitor;
  monitor.add(std::make_unique<ClientHealthAdapter>(client));
  monitor.add(std::make_unique<DbHealthAdapter>(dbForTests()));

  HealthReport before = monitor.check();
  REQUIRE(before.status == HealthStatus::Down);
  REQUIRE(before.detail.find("client:Down") != std::string::npos);

  std::thread acceptor([&server] {
    REQUIRE(server.acceptOnce());
    REQUIRE(socket_io::writePacket(server.peer, connectWelcome()));
  });
  REQUIRE(client.start());
  acceptor.join();

  HealthReport connected = monitor.check();
  REQUIRE(connected.status == HealthStatus::Up);
  REQUIRE(connected.detail.empty());

  client.stop();
  REQUIRE(monitor.check().status == HealthStatus::Down);
}
