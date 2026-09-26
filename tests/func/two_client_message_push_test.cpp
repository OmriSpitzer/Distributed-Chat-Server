/**
 * E2E: two live Clients on one Server — same room MESSAGE push
 *
 * @brief Starts a real Server (server_lib) and two Client instances (client_lib).
 * Both log in as seed users (admin / user), join General; one sends a MESSAGE;
 * the other receives the push.
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
 * 1. two clients same room: MESSAGE push received
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

// winsock singleton instance
Winsock &winsock() {
  static Winsock instance;
  return instance;
}

// port generator
std::uint16_t nextPort() {
  static std::atomic<std::uint16_t> port{28000};
  return port.fetch_add(1);
}

// stub database manager singleton instance
DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    const auto dir = std::filesystem::temp_directory_path() / "dcs-func-e2e-tests";
    std::filesystem::create_directories(dir);
    config::DB_PATH = (dir / "func-e2e.db").string();
    config::NODE_ID = "func-e2e-node";
    return &DatabaseManager::getInstance();
  }();
  return *instance;
}

// wait until predicate is true
template <typename Predicate> bool waitUntil(std::chrono::milliseconds timeout, Predicate pred) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (pred()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  } while (std::chrono::steady_clock::now() < deadline);
  return pred();
}

// config guard
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

// live node
struct LiveNode {
  ConfigGuard guard;
  std::uint16_t port{0};
  std::uint16_t peerPort{0};
  Server server;

  LiveNode() {
    (void)db();
    // shared temp DB can keep online_users from a prior crashed run
    db().clearOnline("admin");
    db().clearOnline("user");
    Logger::clear();
    port = nextPort();
    peerPort = nextPort();
    config::SERVER_HOST = "127.0.0.1";
    config::PORT = port;
    config::PEER_PORT = peerPort;
    config::NODE_ID = "func-e2e-node";
    config::PEERS.clear();

    server.start();
    REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return server.isAlive(); }));
  }

  LiveNode(const LiveNode &) = delete;
  LiveNode &operator=(const LiveNode &) = delete;

  ~LiveNode() {
    // Clients must disconnect before Server::stop (WSACleanup).
    server.stop();
  }
};

// wait for push
bool waitForPush(Client &client, const std::string &author, const std::string &text,
                 std::chrono::milliseconds timeout = std::chrono::seconds(5)) {
  return waitUntil(timeout, [&] {
    for (const ChatLine &line : client.takePendingChatMessages()) {
      if (line.author == author && line.text == text) {
        return true;
      }
    }
    return false;
  });
}

} // namespace

// E2E two clients same room MESSAGE push received
TEST_CASE("E2E two clients same room MESSAGE push received", "[func][e2e][message][flow][slow]") {
  // check winsock is ok
  REQUIRE(winsock().ok);

  // start live node and clients
  LiveNode node;
  Client alice;
  Client bob;

  // start clients and check that they are alive
  REQUIRE(alice.start());
  REQUIRE(bob.start());
  REQUIRE(alice.isAlive());
  REQUIRE(bob.isAlive());

  // login clients and check that they are logged in
  REQUIRE(alice.login("admin", "admin").empty());
  REQUIRE(bob.login("user", "user").empty());
  REQUIRE(alice.getState().isLoggedIn());
  REQUIRE(bob.getState().isLoggedIn());

  // join clients to General and check that they are in the room
  REQUIRE(alice.joinRoom("General").empty());
  REQUIRE(bob.joinRoom("General").empty());
  REQUIRE(alice.getState().currentRoom.has_value());
  REQUIRE(bob.getState().currentRoom.has_value());
  REQUIRE(alice.getState().currentRoom->getName() == "General");
  REQUIRE(bob.getState().currentRoom->getName() == "General");

  // clear any join-side effects before asserting on the chat push
  alice.clearPendingChatMessages();
  bob.clearPendingChatMessages();

  // send message from alice and check that it is received by bob
  const std::string body = "hello from admin";
  REQUIRE(alice.sendMessage(body).empty());

  // wait for push from alice to bob
  REQUIRE(waitForPush(bob, "admin", body));

  // stop clients and check that they are not alive
  bob.stop();
  alice.stop();
  REQUIRE_FALSE(alice.isAlive());
  REQUIRE_FALSE(bob.isAlive());
}
