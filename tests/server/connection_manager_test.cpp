/**
 * ConnectionManager unit tests
 *
 * @brief Includes: listen / stop edges, accept session, heartbeat ping/pong,
 * ignore non-ping heartbeat, sendPacket / closeClient edges, hasSession,
 * rumor with/without gossip, disconnect clears presence, multi-client,
 * stop closes clients, typical connect / login / disconnect flow.
 * @date 13-09-2026
 */

#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/gossip_manager.h"
#include "server/room_manager.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/gossip_payload.h"
#include "utils/models/log_message.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include "utils/socket_io.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>
#include <random>
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
 * 1. destructor without listen
 * 2. stop without start
 * 3. start then stop
 * 4. double start
 * 5. double stop
 * 6. restart after stop
 * 7. accept creates anonymous Lobby session
 * 8. client ping receives pong
 * 9. non-ping heartbeat is ignored
 * 10. sendPacket unknown / closed fails
 * 11. closeClient drops session
 * 12. hasSession after login
 * 13. rumor no-op without gossip; applies with gossip
 * 14. disconnect authenticated clears online (no gossip)
 * 15. disconnect authenticated rumored logout (with gossip)
 * 16. multiple clients
 * 17. stopListening closes live clients
 * 18. typical connect / login / message / disconnect flow
 */

namespace {

constexpr const char *kSrc = "ConnectionManager";
constexpr auto kAcceptWait = std::chrono::seconds(2);
constexpr auto kShortRecv = std::chrono::milliseconds(200);

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

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / "dcs-connection-manager-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / ("node-" + unique("pid") + "-" + std::to_string(rd()) + ".db");
    config::DB_PATH = path.string();
    config::NODE_ID = "cm-test-node";
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

bool setRecvTimeout(SOCKET socket, DWORD milliseconds) {
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&milliseconds),
                    sizeof(milliseconds)) == 0;
}

struct Fixture {
  ConnectionManager connections;
  std::unique_ptr<GossipManager> gossip;
  std::thread acceptThread;
  std::vector<SOCKET> clients;

  Fixture() { (void)db(); }

  Fixture(const Fixture &) = delete;
  Fixture &operator=(const Fixture &) = delete;

  ~Fixture() {
    connections.setGossip(nullptr);
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

  void attachGossip() {
    gossip = std::make_unique<GossipManager>(connections);
    connections.setGossip(gossip.get());
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
    if (::connect(client, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) != 0) {
      closesocket(client);
      return INVALID_SOCKET;
    }

    clients.push_back(client);
    const std::size_t expected = clients.size();
    if (!waitUntil(kAcceptWait, [&] { return connections.getSessions().size() >= expected; })) {
      return INVALID_SOCKET;
    }
    return client;
  }

  User createUser(std::string_view password = "secret") {
    const std::string name = unique("user");
    return db().createUser(name, password, name + "@example.com");
  }

  std::optional<Packet> request(SOCKET client, const Packet &packet) {
    if (!socket_io::writePacket(client, packet)) {
      return std::nullopt;
    }
    return socket_io::readPacket(client);
  }

  // MESSAGE apply broadcasts to the room (including sender) before the reply is sent
  std::optional<Packet> requestMessage(SOCKET client, const Packet &packet) {
    if (!socket_io::writePacket(client, packet)) {
      return std::nullopt;
    }
    std::optional<Packet> push = socket_io::readPacket(client);
    std::optional<Packet> reply = socket_io::readPacket(client);
    if (!push || !reply) {
      return std::nullopt;
    }
    if (push->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS) &&
        push->message == "ok") {
      std::swap(push, reply);
    }
    if (push->type != Packet::PacketType::MESSAGE || push->message != packet.message) {
      return std::nullopt;
    }
    return reply;
  }
};

} // namespace

// 1. destructor without listen
TEST_CASE("ConnectionManager destructor without listen is quiet", "[connection_manager][dtor]") {
  resetLogger();
  REQUIRE(winsock().ok);
  { ConnectionManager connections; }
  REQUIRE(countLogs(LogMessage::Type::INFO) == 0);
}

// 2. stop without start
TEST_CASE("ConnectionManager stop without start warns", "[connection_manager][stop][edge]") {
  resetLogger();
  REQUIRE(winsock().ok);
  ConnectionManager connections;
  connections.stopListening();
  REQUIRE(countLogs(LogMessage::Type::WARNING, "Not listening") == 1);
  REQUIRE_FALSE(connections.isListening());
}

// 3. start then stop
TEST_CASE("ConnectionManager start then stop", "[connection_manager][start][stop]") {
  resetLogger();
  REQUIRE(winsock().ok);
  ConnectionManager connections;
  REQUIRE(connections.startListening(0));
  REQUIRE(connections.isListening());
  REQUIRE(connections.getListeningSocket() != -1);
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Listening on port"));

  connections.stopListening();
  REQUIRE_FALSE(connections.isListening());
  REQUIRE(connections.getListeningSocket() == -1);
}

// 4. double start
TEST_CASE("ConnectionManager double start refuses", "[connection_manager][start][edge]") {
  resetLogger();
  REQUIRE(winsock().ok);
  ConnectionManager connections;
  REQUIRE(connections.startListening(0));
  REQUIRE_FALSE(connections.startListening(0));
  REQUIRE(countLogs(LogMessage::Type::WARNING, "Already listening") == 1);
  connections.stopListening();
}

// 5. double stop
TEST_CASE("ConnectionManager double stop warns once then again",
          "[connection_manager][stop][edge]") {
  resetLogger();
  REQUIRE(winsock().ok);
  ConnectionManager connections;
  REQUIRE(connections.startListening(0));
  connections.stopListening();
  connections.stopListening();
  REQUIRE(countLogs(LogMessage::Type::WARNING, "Not listening") >= 1);
}

// 6. restart after stop
TEST_CASE("ConnectionManager restart after stop", "[connection_manager][restart]") {
  resetLogger();
  REQUIRE(winsock().ok);
  ConnectionManager connections;
  REQUIRE(connections.startListening(0));
  connections.stopListening();
  REQUIRE(connections.startListening(0));
  REQUIRE(connections.isListening());
  connections.stopListening();
}

// 7. accept creates anonymous Lobby session
TEST_CASE("ConnectionManager accept creates anonymous Lobby session",
          "[connection_manager][accept]") {
  resetLogger();
  REQUIRE(winsock().ok);
  Fixture fx;
  REQUIRE(fx.listen());
  REQUIRE(fx.connectClient() != INVALID_SOCKET);

  auto sessions = fx.connections.getSessions();
  REQUIRE(sessions.size() == 1);
  const auto &session = sessions.begin()->second;
  REQUIRE(session);
  REQUIRE_FALSE(session->isAuthenticated());
  REQUIRE(session->getRoom().getName() == RoomManager::LOBBY.getName());
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "New client connected"));
}

// 8. client ping receives pong
TEST_CASE("ConnectionManager client ping receives pong", "[connection_manager][heartbeat]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, 2000));

  Packet ping("c", "server", Packet::PacketType::HEARTBEAT, "", "ping");
  const auto res = fx.request(client, ping);
  REQUIRE(res.has_value());
  REQUIRE(res->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(res->message == "pong");
  REQUIRE(res->sender == "server");
}

// 9. non-ping heartbeat is ignored
TEST_CASE("ConnectionManager ignores non-ping heartbeat", "[connection_manager][heartbeat][edge]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, static_cast<DWORD>(kShortRecv.count())));

  Packet noise("c", "server", Packet::PacketType::HEARTBEAT, "", "server-ping");
  REQUIRE(socket_io::writePacket(client, noise));
  REQUIRE_FALSE(socket_io::readPacket(client).has_value());

  // session still alive for a real ping
  REQUIRE(setRecvTimeout(client, 2000));
  Packet ping("c", "server", Packet::PacketType::HEARTBEAT, "", "ping");
  const auto res = fx.request(client, ping);
  REQUIRE(res.has_value());
  REQUIRE(res->message == "pong");
}

// 10. sendPacket unknown / closed fails
TEST_CASE("ConnectionManager sendPacket edges", "[connection_manager][send][edge]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);

  Packet hello("server", "c", Packet::PacketType::HEARTBEAT, "", "hi");
  REQUIRE_FALSE(fx.connections.sendPacket(999999, hello));

  auto sessions = fx.connections.getSessions();
  REQUIRE(sessions.size() == 1);
  const int fd = sessions.begin()->first;
  sessions.begin()->second->markClosed();
  REQUIRE_FALSE(fx.connections.sendPacket(fd, hello));
  (void)client;
}

// 11. closeClient drops session
TEST_CASE("ConnectionManager closeClient removes session", "[connection_manager][close]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, 2000));

  auto sessions = fx.connections.getSessions();
  REQUIRE(sessions.size() == 1);
  const int fd = sessions.begin()->first;
  fx.connections.closeClient(fd);

  REQUIRE(waitUntil(kAcceptWait, [&] {
    return fx.connections.getSessions().empty() &&
           hasLogContaining(LogMessage::Type::INFO, "Client disconnected");
  }));
}

// 12. hasSession after login
TEST_CASE("ConnectionManager hasSession after login", "[connection_manager][hasSession]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  fx.attachGossip();
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, 3000));

  const User user = fx.createUser("secret");
  REQUIRE_FALSE(fx.connections.hasSession(user));

  Packet login(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "secret");
  const auto res = fx.request(client, login);
  REQUIRE(res.has_value());
  REQUIRE(res->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(fx.connections.hasSession(user));
  REQUIRE(db().isUserOnline(user.getUsername()));
}

// 13. rumor no-op without gossip; applies with gossip
TEST_CASE("ConnectionManager rumor with and without gossip", "[connection_manager][rumor]") {
  REQUIRE(winsock().ok);
  (void)db();
  const std::string user = unique("rumor");
  REQUIRE_NOTHROW(db().createUser(user, "secret", user + "@example.com"));

  ConnectionManager bare;
  const std::string eventId = unique("eid");
  const std::string payload =
      gossip_payload::encode("LOGIN", eventId, user, config::NODE_ID, "0");
  Packet event(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, "", payload);
  REQUIRE_NOTHROW(bare.rumor(event));
  REQUIRE_FALSE(db().isUserOnline(user));

  Fixture fx;
  fx.attachGossip();
  fx.connections.rumor(event);
  REQUIRE(db().isUserOnline(user));
  db().clearOnline(user);
}

// 14. disconnect authenticated clears online (no gossip)
TEST_CASE("ConnectionManager disconnect clears online without gossip",
          "[connection_manager][disconnect][edge]") {
  REQUIRE(winsock().ok);
  Fixture fx; // no gossip attached
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, 3000));

  const User user = fx.createUser("secret");
  // login without gossip still authenticates session, but presence needs manual set for assert
  Packet login(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "secret");
  const auto res = fx.request(client, login);
  REQUIRE(res.has_value());
  REQUIRE(res->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));

  // without gossip, LOGIN rumor is a no-op — mark online as production would via gossip
  db().setOnline(user.getUsername(), config::NODE_ID);
  REQUIRE(db().isUserOnline(user.getUsername()));

  closesocket(client);
  fx.clients.back() = INVALID_SOCKET;
  REQUIRE(waitUntil(kAcceptWait, [&] { return fx.connections.getSessions().empty(); }));
  REQUIRE_FALSE(db().isUserOnline(user.getUsername()));
}

// 15. disconnect authenticated rumored logout (with gossip)
TEST_CASE("ConnectionManager disconnect rumored logout with gossip",
          "[connection_manager][disconnect]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  fx.attachGossip();
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, 3000));

  const User user = fx.createUser("secret");
  Packet login(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "secret");
  REQUIRE(fx.request(client, login)->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(db().isUserOnline(user.getUsername()));

  closesocket(client);
  fx.clients.back() = INVALID_SOCKET;
  REQUIRE(waitUntil(kAcceptWait, [&] {
    return fx.connections.getSessions().empty() && !db().isUserOnline(user.getUsername());
  }));
}

// 16. multiple clients
TEST_CASE("ConnectionManager accepts multiple clients", "[connection_manager][accept]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  REQUIRE(fx.listen());
  REQUIRE(fx.connectClient() != INVALID_SOCKET);
  REQUIRE(fx.connectClient() != INVALID_SOCKET);
  REQUIRE(fx.connectClient() != INVALID_SOCKET);
  REQUIRE(fx.connections.getSessions().size() == 3);
}

// 17. stopListening closes live clients
TEST_CASE("ConnectionManager stopListening closes clients", "[connection_manager][stop]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, 2000));

  fx.connections.stopListening();
  REQUIRE_FALSE(fx.connections.isListening());
  REQUIRE(waitUntil(kAcceptWait, [&] { return fx.connections.getSessions().empty(); }));
  REQUIRE_FALSE(socket_io::readPacket(client).has_value());
}

// 18. typical connect / login / message / disconnect flow
TEST_CASE("ConnectionManager typical connect login message disconnect flow",
          "[connection_manager][flow]") {
  REQUIRE(winsock().ok);
  Fixture fx;
  fx.attachGossip();
  REQUIRE(fx.listen());
  const SOCKET client = fx.connectClient();
  REQUIRE(client != INVALID_SOCKET);
  REQUIRE(setRecvTimeout(client, 5000));

  const User user = fx.createUser("secret");
  Packet login(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "secret");
  REQUIRE(fx.request(client, login)->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));

  Packet msg(user.getUsername(), "server", Packet::PacketType::MESSAGE, "Lobby", "cm-hi|ok");
  const auto msgRes = fx.requestMessage(client, msg);
  REQUIRE(msgRes.has_value());
  REQUIRE(msgRes->responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(msgRes->message == "ok");

  bool found = false;
  for (const auto &m : db().loadHistory(RoomManager::LOBBY.getId())) {
    if (m.getContent() == "cm-hi|ok" && m.getFrom().getUsername() == user.getUsername()) {
      found = true;
      break;
    }
  }
  REQUIRE(found);

  closesocket(client);
  fx.clients.back() = INVALID_SOCKET;
  REQUIRE(waitUntil(kAcceptWait, [&] {
    return fx.connections.getSessions().empty() && !db().isUserOnline(user.getUsername());
  }));
}
