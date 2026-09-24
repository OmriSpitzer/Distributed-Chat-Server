/**
 * E2E: Lobby leave rules, unknown room, logout/login, double-login
 *
 * @brief Live Server + Client(s). Seed accounts admin / user from init.sql.
 * @date 21-09-2026
 */

#include "client/client.h"
#include "config/config.h"
#include "server/database_manager.h"
#include "server/server.h"
#include "utils/logger/logger.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

/**
 * 1. cannot leave Lobby; leave General returns to Lobby
 * 2. join unknown room fails; current room unchanged
 * 3. logout then login again on the same connection
 * 4. second client login with same user is rejected
 */

// seed accounts from init.sql
constexpr const char *kAdminUser = "admin";
constexpr const char *kAdminPass = "admin";
constexpr const char *kUserUser = "user";
constexpr const char *kUserPass = "user";

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

std::uint16_t nextPort() {
  static std::atomic<std::uint16_t> port{29000};
  return port.fetch_add(1);
}

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    const auto dir = std::filesystem::temp_directory_path() / "dcs-func-e2e-tests";
    std::filesystem::create_directories(dir);
    config::DB_PATH = (dir / "func-session-rules.db").string();
    config::NODE_ID = "func-session-node";
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
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  } while (std::chrono::steady_clock::now() < deadline);
  return pred();
}

struct ConfigGuard {
  std::string host = config::SERVER_HOST;
  std::uint16_t port = config::PORT;
  std::uint16_t peerPort = config::PEER_PORT;
  std::string nodeId = config::NODE_ID;
  std::string dbPath = config::DB_PATH;
  std::vector<std::string> peers = config::PEERS;

  ~ConfigGuard() {
    config::SERVER_HOST = host;
    config::PORT = port;
    config::PEER_PORT = peerPort;
    config::NODE_ID = nodeId;
    config::DB_PATH = dbPath;
    config::PEERS = peers;
  }
};

struct LiveNode {
  ConfigGuard guard;
  std::uint16_t port{0};
  std::uint16_t peerPort{0};
  Server server;

  LiveNode() {
    (void)db();
    // shared temp DB can keep online_users from a prior crashed run
    db().clearOnline(kAdminUser);
    db().clearOnline(kUserUser);
    Logger::clear();
    port = nextPort();
    peerPort = nextPort();
    config::SERVER_HOST = "127.0.0.1";
    config::PORT = port;
    config::PEER_PORT = peerPort;
    config::NODE_ID = "func-session-node";
    config::PEERS.clear();

    server.start();
    REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return server.isAlive(); }));
  }

  LiveNode(const LiveNode &) = delete;
  LiveNode &operator=(const LiveNode &) = delete;

  ~LiveNode() { server.stop(); }
};

std::string roomName(const Client &client) {
  const auto &room = client.getState().currentRoom;
  REQUIRE(room.has_value());
  return room->getName();
}

} // namespace

// 1. cannot leave Lobby; leave General returns to Lobby
TEST_CASE("E2E join leave Lobby rules", "[func][e2e][room][flow][slow]") {
  REQUIRE(winsock().ok);
  LiveNode node;

  Client client;
  REQUIRE(client.start());
  REQUIRE(client.login(kAdminUser, kAdminPass).empty());
  REQUIRE(roomName(client) == "Lobby");

  const std::string leaveLobby = client.leaveRoom();
  REQUIRE_FALSE(leaveLobby.empty());
  REQUIRE(leaveLobby == "Cannot leave the Lobby.");
  REQUIRE(roomName(client) == "Lobby");

  REQUIRE(client.joinRoom("General").empty());
  REQUIRE(roomName(client) == "General");

  REQUIRE(client.leaveRoom().empty());
  REQUIRE(roomName(client) == "Lobby");

  REQUIRE(client.leaveRoom() == "Cannot leave the Lobby.");

  client.stop();
}

// 2. join unknown room fails; current room unchanged
TEST_CASE("E2E join unknown room returns error", "[func][e2e][room][edge][slow]") {
  REQUIRE(winsock().ok);
  LiveNode node;

  Client client;
  REQUIRE(client.start());
  REQUIRE(client.login(kUserUser, kUserPass).empty());
  REQUIRE(roomName(client) == "Lobby");

  const std::string err = client.joinRoom("NoSuchRoom");
  REQUIRE_FALSE(err.empty());
  REQUIRE(err.find("unknown room") != std::string::npos);
  REQUIRE(roomName(client) == "Lobby");

  client.stop();
}

// 3. logout then login again on the same connection
TEST_CASE("E2E logout then login again", "[func][e2e][auth][flow][slow]") {
  REQUIRE(winsock().ok);
  LiveNode node;

  Client client;
  REQUIRE(client.start());
  REQUIRE(client.login(kAdminUser, kAdminPass).empty());
  REQUIRE(client.getState().isLoggedIn());

  REQUIRE(client.logout().empty());
  REQUIRE_FALSE(client.getState().isLoggedIn());
  REQUIRE(client.isAlive());

  REQUIRE(client.login(kAdminUser, kAdminPass).empty());
  REQUIRE(client.getState().isLoggedIn());
  REQUIRE(roomName(client) == "Lobby");

  client.stop();
}

// 4. second client login with same user is rejected
TEST_CASE("E2E double login rejected", "[func][e2e][auth][edge][slow]") {
  REQUIRE(winsock().ok);
  LiveNode node;

  Client first;
  Client second;
  REQUIRE(first.start());
  REQUIRE(second.start());

  REQUIRE(first.login(kAdminUser, kAdminPass).empty());
  REQUIRE(first.getState().isLoggedIn());

  const std::string err = second.login(kAdminUser, kAdminPass);
  REQUIRE_FALSE(err.empty());
  REQUIRE(err.find("already logged in") != std::string::npos);
  REQUIRE_FALSE(second.getState().isLoggedIn());
  REQUIRE(first.getState().isLoggedIn());

  // other seed account still works on the second socket
  REQUIRE(second.login(kUserUser, kUserPass).empty());
  REQUIRE(second.getState().isLoggedIn());

  second.stop();
  first.stop();
}
