/**
 * GossipManager unit tests
 *
 * @brief Includes: construct / destroy, start / stop edges, rumor empty / malformed /
 * LOGIN / LOGOUT / USER_CREATED / ROOM_JOIN / ROOM_LEAVE / MESSAGE, duplicate event id,
 * peer HELLO register / reject, event before HELLO, digest→pull→event, dial bad addr /
 * dial success, concurrent rumor, typical presence flow.
 * @date 13-09-2026
 */

#include "config/config.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/gossip_manager.h"
#include "server/room_manager.h"
#include "utils/gossip_payload.h"
#include "utils/models/log_message.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
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
 * 1. destructor without start
 * 2. stop without start
 * 3. start then stop
 * 4. double start
 * 5. double stop
 * 6. restart after stop
 * 7. rumor empty event id is a no-op
 * 8. rumor LOGIN / LOGOUT
 * 9. rumor USER_CREATED
 * 10. rumor ROOM_JOIN / ROOM_LEAVE
 * 11. rumor MESSAGE persists
 * 12. rumor duplicate event id ignored
 * 13. rumor malformed payload
 * 14. peer HELLO registers
 * 15. peer HELLO self / duplicate rejected
 * 16. peer EVENT before HELLO ignored
 * 17. peer EVENT after HELLO applied
 * 18. peer DIGEST triggers PULL then EVENT
 * 19. dial bad address warns
 * 20. dial connects to peer
 * 21. concurrent rumor
 * 22. typical LOGIN / MESSAGE / LOGOUT flow
 */

namespace {

constexpr const char *kSrc = "GossipManager";
constexpr auto kDialWait = std::chrono::seconds(5);
constexpr auto kAntiEntropyWait = std::chrono::seconds(5);

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
  static std::atomic<std::uint16_t> port{21000};
  return port.fetch_add(1);
}

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / "dcs-gossip-manager-tests";
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
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

Packet makeEvent(std::string_view type, std::string_view eventId, std::string_view username,
                 std::string_view content, std::string_view field5, std::string_view room = "") {
  const std::string payload = gossip_payload::encode(type, eventId, username, content, field5);
  return Packet(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, room, payload);
}

struct GossipFixture {
  ConnectionManager connections;
  std::unique_ptr<GossipManager> gossip;
  std::string nodeId;
  std::uint16_t peerPort{0};
  std::vector<SOCKET> peerClients;

  GossipFixture() {
    db();
    nodeId = unique("node");
    peerPort = nextPort();
    config::NODE_ID = nodeId;
    config::PEER_PORT = peerPort;
    config::PEERS.clear();
    gossip = std::make_unique<GossipManager>(connections);
  }

  GossipFixture(const GossipFixture &) = delete;
  GossipFixture &operator=(const GossipFixture &) = delete;

  ~GossipFixture() {
    if (gossip) {
      gossip->stop();
    }
    for (SOCKET socket : peerClients) {
      if (socket != INVALID_SOCKET) {
        socket_io::close(socket);
      }
    }
  }

  bool start() {
    resetLogger();
    gossip->start();
    return waitUntil(std::chrono::seconds(2), [&] {
      return countLogs(LogMessage::Type::INFO,
                       "Listening on port " + std::to_string(peerPort)) >= 1;
    });
  }

  // act as a remote peer dialing into this GossipManager
  SOCKET connectAsPeer() {
    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET) {
      return INVALID_SOCKET;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(peerPort);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(client, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) != 0) {
      socket_io::close(client);
      return INVALID_SOCKET;
    }

    peerClients.push_back(client);
    REQUIRE(setRecvTimeout(client, 2000));
    return client;
  }

  bool handshake(SOCKET client, const std::string &remoteNodeId) {
    auto hello = socket_io::readPacket(client);
    if (!hello || hello->type != Packet::PacketType::GOSSIP_HELLO) {
      return false;
    }

    Packet reply(remoteNodeId, "*", Packet::PacketType::GOSSIP_HELLO);
    if (!socket_io::writePacket(client, reply)) {
      return false;
    }

    return waitUntil(std::chrono::seconds(2),
                     [&] { return hasLogContaining(LogMessage::Type::INFO, "Peer registered: "); });
  }
};

} // namespace

// 1. destructor without start
TEST_CASE("GossipManager destructor without start does not log Stopped",
          "[gossip_manager][dtor]") {
  resetLogger();
  REQUIRE(winsock().ok);
  db();

  {
    ConnectionManager connections;
    config::NODE_ID = unique("node");
    config::PEER_PORT = nextPort();
    config::PEERS.clear();
    GossipManager gossip(connections);
  }

  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 0);
}

// 2. stop without start
TEST_CASE("GossipManager stop without start is a no-op", "[gossip_manager][stop]") {
  resetLogger();
  REQUIRE(winsock().ok);
  db();

  ConnectionManager connections;
  config::NODE_ID = unique("node");
  config::PEER_PORT = nextPort();
  config::PEERS.clear();
  GossipManager gossip(connections);
  gossip.stop();
  gossip.stop();

  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 0);
}

// 3. start then stop
TEST_CASE("GossipManager start then stop logs Listening and Stopped",
          "[gossip_manager][start][stop]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  fixture.gossip->stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 4. double start
TEST_CASE("GossipManager double start listens once", "[gossip_manager][start][edge]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  fixture.gossip->start();
  fixture.gossip->start();

  REQUIRE(countLogs(LogMessage::Type::INFO,
                    "Listening on port " + std::to_string(fixture.peerPort)) == 1);
  fixture.gossip->stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 5. double stop
TEST_CASE("GossipManager double stop logs Stopped once", "[gossip_manager][stop][edge]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  fixture.gossip->stop();
  fixture.gossip->stop();
  fixture.gossip->stop();

  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 6. restart after stop
TEST_CASE("GossipManager restart after stop listens again", "[gossip_manager][restart]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());
  fixture.gossip->stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);

  resetLogger();
  fixture.gossip->start();
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] {
    return countLogs(LogMessage::Type::INFO,
                     "Listening on port " + std::to_string(fixture.peerPort)) >= 1;
  }));
  fixture.gossip->stop();
  REQUIRE(countLogs(LogMessage::Type::INFO, "Stopped") == 1);
}

// 7. rumor empty event id is a no-op
TEST_CASE("GossipManager rumor empty event id is a no-op", "[gossip_manager][rumor][edge]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  Packet empty(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, "", "");
  Packet emptyId(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, "",
                 gossip_payload::encode("LOGIN", "", "user", "node", "0"));
  fixture.gossip->rumor(empty);
  fixture.gossip->rumor(emptyId);

  REQUIRE_FALSE(hasLogContaining(LogMessage::Type::INFO, "Applied"));
  fixture.gossip->stop();
}

// 8. rumor LOGIN / LOGOUT
TEST_CASE("GossipManager rumor LOGIN and LOGOUT update presence",
          "[gossip_manager][rumor][login][logout]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string user = unique("user");
  const std::string loginId = unique("login");
  fixture.gossip->rumor(makeEvent("LOGIN", loginId, user, "remote-node", "0"));

  REQUIRE(waitUntil(std::chrono::seconds(1), [&] { return db().isUserOnline(user); }));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Applied LOGIN for " + user));

  const std::string logoutId = unique("logout");
  fixture.gossip->rumor(makeEvent("LOGOUT", logoutId, user, "remote-node", "0"));
  REQUIRE(waitUntil(std::chrono::seconds(1), [&] { return !db().isUserOnline(user); }));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Applied LOGOUT for " + user));

  fixture.gossip->stop();
}

// 9. rumor USER_CREATED
TEST_CASE("GossipManager rumor USER_CREATED inserts user", "[gossip_manager][rumor][user]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string user = unique("created");
  const std::string email = user + "@example.com";
  const std::string eventId = unique("uc");
  fixture.gossip->rumor(makeEvent("USER_CREATED", eventId, user, "secret", email));

  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return db().userExists(user); }));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Applied USER_CREATED for " + user));

  // duplicate apply for same username (new event id) should still succeed
  fixture.gossip->rumor(makeEvent("USER_CREATED", unique("uc2"), user, "secret", email));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Applied USER_CREATED for " + user));

  fixture.gossip->stop();
}

// 10b. rumor ROOM_CREATED seeds creator allow list; ROOM_ACL_ADD invites
TEST_CASE("GossipManager rumor ROOM_CREATED and ROOM_ACL_ADD", "[gossip_manager][rumor][acl]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string creator = unique("owner");
  const std::string creatorEmail = creator + "@example.com";
  const std::string guest = unique("guest");
  const std::string guestEmail = guest + "@example.com";
  const std::string roomName = unique("vault");

  REQUIRE_NOTHROW(db().createUser(creator, "secret", creatorEmail));
  REQUIRE_NOTHROW(db().createUser(guest, "secret", guestEmail));

  fixture.gossip->rumor(makeEvent("ROOM_CREATED", unique("rc"), creator, "Other", "PRIVATE",
                                  roomName));

  REQUIRE(waitUntil(std::chrono::seconds(2), [&] {
    return RoomManager::getInstance().getRoom(roomName).has_value();
  }));

  const auto created = RoomManager::getInstance().getRoom(roomName);
  REQUIRE(created.has_value());
  REQUIRE(created->getPrivacy() == Room::Privacy::PRIVATE);
  REQUIRE(db().isAllowListCreator(created->getId(), creatorEmail));
  REQUIRE(db().isAllowed(created->getId(), creatorEmail));
  REQUIRE_FALSE(db().isAllowed(created->getId(), guestEmail));

  fixture.gossip->rumor(
      makeEvent("ROOM_ACL_ADD", unique("acl"), guest, "0", "", roomName));
  REQUIRE(waitUntil(std::chrono::seconds(2),
                    [&] { return db().isAllowed(created->getId(), guestEmail); }));
  REQUIRE_FALSE(db().isAllowListCreator(created->getId(), guestEmail));

  fixture.gossip->stop();
}

TEST_CASE("GossipManager rumor ROOM_DELETED and ROOM_KICK", "[gossip_manager][rumor][admin]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string creator = unique("c");
  const std::string creatorEmail = creator + "@example.com";
  const std::string guest = unique("g");
  const std::string guestEmail = guest + "@example.com";
  REQUIRE_NOTHROW(db().createUser(creator, "secret", creatorEmail));
  REQUIRE_NOTHROW(db().createUser(guest, "secret", guestEmail));

  const std::string roomName = unique("doom");
  fixture.gossip->rumor(makeEvent("ROOM_CREATED", unique("rc"), creator, "Other", "PRIVATE",
                                  roomName));
  REQUIRE(waitUntil(std::chrono::seconds(2),
                    [&] { return RoomManager::getInstance().getRoom(roomName).has_value(); }));
  auto created = RoomManager::getInstance().getRoom(roomName);
  REQUIRE(created.has_value());

  fixture.gossip->rumor(makeEvent("ROOM_ACL_ADD", unique("acl"), guest, "0", "", roomName));
  REQUIRE(waitUntil(std::chrono::seconds(2),
                    [&] { return db().isAllowed(created->getId(), guestEmail); }));

  fixture.gossip->rumor(makeEvent("ROOM_KICK", unique("kick"), guest, fixture.nodeId, "", roomName));
  REQUIRE(waitUntil(std::chrono::seconds(2),
                    [&] { return !db().isAllowed(created->getId(), guestEmail); }));

  fixture.gossip->rumor(
      makeEvent("ROOM_DELETED", unique("rd"), creator, fixture.nodeId, "", roomName));
  REQUIRE(waitUntil(std::chrono::seconds(2),
                    [&] { return !RoomManager::getInstance().getRoom(roomName).has_value(); }));

  fixture.gossip->stop();
}

// 10. rumor ROOM_JOIN / ROOM_LEAVE
TEST_CASE("GossipManager rumor ROOM_JOIN and ROOM_LEAVE", "[gossip_manager][rumor][room]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string user = unique("joiner");
  REQUIRE_NOTHROW(db().createUser(user, "secret", user + "@example.com"));
  REQUIRE_NOTHROW(db().setMembership(user, 1, "n1"));

  fixture.gossip->rumor(
      makeEvent("ROOM_JOIN", unique("rj"), user, "n2", "Lobby", "General"));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Applied ROOM_JOIN for " + user));

  fixture.gossip->rumor(makeEvent("ROOM_LEAVE", unique("rl"), user, "n2", "General", "General"));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Applied ROOM_LEAVE for " + user));
  REQUIRE_NOTHROW(db().clearAllMembership(user));

  fixture.gossip->stop();
}

// 11. rumor MESSAGE persists
TEST_CASE("GossipManager rumor MESSAGE saves history", "[gossip_manager][rumor][message]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string user = unique("chat");
  const std::string email = user + "@example.com";
  REQUIRE_NOTHROW(db().createUser(user, "secret", email));

  const std::string msgId = unique("msg");
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  fixture.gossip->rumor(makeEvent("MESSAGE", msgId, user, "hello-gossip", ts, "Lobby"));

  const std::string pipeMsgId = unique("msgpipe");
  fixture.gossip->rumor(
      makeEvent("MESSAGE", pipeMsgId, user, "hello|with|pipes", ts, "Lobby"));

  REQUIRE(waitUntil(std::chrono::seconds(2), [&] {
    const auto history = db().loadHistory(1);
    bool plain = false;
    bool pipes = false;
    for (const auto &m : history) {
      if (m.getId() == msgId && m.getContent() == "hello-gossip") {
        plain = true;
      }
      if (m.getId() == pipeMsgId && m.getContent() == "hello|with|pipes") {
        pipes = true;
      }
    }
    return plain && pipes;
  }));

  fixture.gossip->stop();
}

// 12. rumor duplicate event id ignored
TEST_CASE("GossipManager duplicate rumor event id is ignored",
          "[gossip_manager][rumor][edge]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string user = unique("dup");
  const std::string eventId = unique("once");
  auto login = makeEvent("LOGIN", eventId, user, "node-a", "0");
  fixture.gossip->rumor(login);
  REQUIRE(waitUntil(std::chrono::seconds(1), [&] { return db().isUserOnline(user); }));

  db().clearOnline(user);
  REQUIRE_FALSE(db().isUserOnline(user));

  fixture.gossip->rumor(login); // same id — must not re-apply
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  REQUIRE_FALSE(db().isUserOnline(user));

  fixture.gossip->stop();
}

// 13. rumor malformed payload
TEST_CASE("GossipManager rumor malformed payload does not crash",
          "[gossip_manager][rumor][edge]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  // type + eventId only (missing remaining fields) — rumor accepts id, apply rejects body
  const std::string truncatedId = unique("bad");
  std::string truncated = gossip_payload::encode("LOGIN", truncatedId, "", "", "");
  truncated.resize(truncated.size() - 12); // drop three empty trailing fields
  Packet bad(config::NODE_ID, "*", Packet::PacketType::GOSSIP_EVENT, "", truncated);
  Packet badLogin = makeEvent("LOGIN", unique("bad2"), "", "", "0");
  Packet badTs = makeEvent("MESSAGE", unique("bad3"), "u", "body", "not-a-ts", "Lobby");

  REQUIRE_NOTHROW(fixture.gossip->rumor(bad));
  REQUIRE_NOTHROW(fixture.gossip->rumor(badLogin));
  REQUIRE_NOTHROW(fixture.gossip->rumor(badTs));
  REQUIRE(hasLogContaining(LogMessage::Type::WARNING, "Malformed"));

  fixture.gossip->stop();
}

// 14. peer HELLO registers
TEST_CASE("GossipManager peer HELLO registers remote node", "[gossip_manager][peer][hello]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  SOCKET peer = fixture.connectAsPeer();
  REQUIRE(peer != INVALID_SOCKET);
  const std::string remote = unique("peer");
  REQUIRE(fixture.handshake(peer, remote));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Peer registered: " + remote));

  fixture.gossip->stop();
}

// 15. peer HELLO self / duplicate rejected
TEST_CASE("GossipManager peer HELLO rejects self and duplicate node id",
          "[gossip_manager][peer][hello][edge]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  SOCKET selfPeer = fixture.connectAsPeer();
  REQUIRE(selfPeer != INVALID_SOCKET);
  auto hello = socket_io::readPacket(selfPeer);
  REQUIRE(hello);
  REQUIRE(hello->type == Packet::PacketType::GOSSIP_HELLO);
  REQUIRE(socket_io::writePacket(selfPeer,
                                 Packet(fixture.nodeId, "*", Packet::PacketType::GOSSIP_HELLO)));
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] {
    return hasLogContaining(LogMessage::Type::WARNING, "Rejected peer HELLO from ");
  }));

  SOCKET peerA = fixture.connectAsPeer();
  REQUIRE(peerA != INVALID_SOCKET);
  const std::string remote = unique("duppeer");
  REQUIRE(fixture.handshake(peerA, remote));

  SOCKET peerB = fixture.connectAsPeer();
  REQUIRE(peerB != INVALID_SOCKET);
  auto helloB = socket_io::readPacket(peerB);
  REQUIRE(helloB);
  REQUIRE(socket_io::writePacket(peerB, Packet(remote, "*", Packet::PacketType::GOSSIP_HELLO)));
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] {
    return hasLogContaining(LogMessage::Type::WARNING, "Rejected peer HELLO from " + remote);
  }));

  fixture.gossip->stop();
}

// 16. peer EVENT before HELLO ignored
TEST_CASE("GossipManager ignores EVENT before HELLO", "[gossip_manager][peer][event][edge]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  SOCKET peer = fixture.connectAsPeer();
  REQUIRE(peer != INVALID_SOCKET);
  auto hello = socket_io::readPacket(peer);
  REQUIRE(hello);

  const std::string user = unique("early");
  const std::string eventId = unique("earlyevt");
  REQUIRE(socket_io::writePacket(peer, makeEvent("LOGIN", eventId, user, "n", "0")));
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  REQUIRE_FALSE(db().isUserOnline(user));

  fixture.gossip->stop();
}

// 17. peer EVENT after HELLO applied
TEST_CASE("GossipManager applies EVENT after HELLO", "[gossip_manager][peer][event]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  SOCKET peer = fixture.connectAsPeer();
  REQUIRE(peer != INVALID_SOCKET);
  REQUIRE(fixture.handshake(peer, unique("peer")));

  const std::string user = unique("remoteuser");
  const std::string eventId = unique("remoteevt");
  REQUIRE(socket_io::writePacket(peer, makeEvent("LOGIN", eventId, user, "peer-node", "0")));
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return db().isUserOnline(user); }));
  REQUIRE(hasLogContaining(LogMessage::Type::INFO, "Got event " + eventId));

  fixture.gossip->stop();
}

// 18. peer DIGEST triggers PULL then EVENT
TEST_CASE("GossipManager DIGEST pull fills missing events", "[gossip_manager][peer][digest]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  SOCKET peer = fixture.connectAsPeer();
  REQUIRE(peer != INVALID_SOCKET);
  REQUIRE(fixture.handshake(peer, unique("peer")));

  const std::string user = unique("digestuser");
  const std::string missingId = unique("missing");
  Packet digest(config::NODE_ID, "*", Packet::PacketType::GOSSIP_DIGEST, "", missingId);
  REQUIRE(socket_io::writePacket(peer, digest));

  std::optional<Packet> pull;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    auto pkt = socket_io::readPacket(peer);
    if (!pkt) {
      break;
    }
    if (pkt->type == Packet::PacketType::GOSSIP_PULL) {
      pull = pkt;
      break;
    }
  }
  REQUIRE(pull);
  REQUIRE(pull->message.find(missingId) != std::string::npos);

  REQUIRE(socket_io::writePacket(peer, makeEvent("LOGIN", missingId, user, "peer-node", "0")));
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return db().isUserOnline(user); }));

  fixture.gossip->stop();
}

// 19. dial bad address warns
TEST_CASE("GossipManager dial bad address logs warning", "[gossip_manager][dial][edge]") {
  REQUIRE(winsock().ok);
  db();
  resetLogger();

  ConnectionManager connections;
  config::NODE_ID = unique("node");
  config::PEER_PORT = nextPort();
  config::PEERS = {"not-a-valid-address", "127.0.0.1:0", ":1234"};
  GossipManager gossip(connections);
  gossip.start();

  REQUIRE(waitUntil(kDialWait, [&] {
    return hasLogContaining(LogMessage::Type::WARNING, "Bad peer address");
  }));

  gossip.stop();
  config::PEERS.clear();
}

// 20. dial connects to peer
TEST_CASE("GossipManager dial connects to listening peer", "[gossip_manager][dial]") {
  REQUIRE(winsock().ok);
  db();

  const std::uint16_t listenPort = nextPort();
  SOCKET listenFd = socket_io::listenTo(listenPort);
  REQUIRE(listenFd != INVALID_SOCKET);

  std::atomic<bool> gotHello{false};
  std::thread acceptor([&] {
    SOCKET peer = socket_io::acceptFrom(listenFd);
    if (peer == INVALID_SOCKET) {
      return;
    }
    setRecvTimeout(peer, 3000);
    auto hello = socket_io::readPacket(peer);
    if (hello && hello->type == Packet::PacketType::GOSSIP_HELLO) {
      gotHello = true;
      Packet reply(unique("listener"), "*", Packet::PacketType::GOSSIP_HELLO);
      socket_io::writePacket(peer, reply);
    }
    socket_io::close(peer);
  });

  resetLogger();
  ConnectionManager connections;
  config::NODE_ID = unique("dialer");
  config::PEER_PORT = nextPort();
  config::PEERS = {"127.0.0.1:" + std::to_string(listenPort)};
  GossipManager gossip(connections);
  gossip.start();

  REQUIRE(waitUntil(kDialWait, [&] {
    return gotHello.load() || hasLogContaining(LogMessage::Type::INFO, "Dialed peer ");
  }));

  gossip.stop();
  socket_io::close(listenFd);
  if (acceptor.joinable()) {
    acceptor.join();
  }
  config::PEERS.clear();
  REQUIRE(gotHello.load());
}

// 21. concurrent rumor
TEST_CASE("GossipManager concurrent rumor is safe", "[gossip_manager][rumor][concurrent]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  constexpr int kN = 12;
  std::vector<std::thread> threads;
  threads.reserve(kN);
  std::vector<std::string> users;
  users.reserve(kN);
  for (int i = 0; i < kN; ++i) {
    users.push_back(unique("cuser"));
  }

  for (int i = 0; i < kN; ++i) {
    threads.emplace_back([&, i] {
      fixture.gossip->rumor(
          makeEvent("LOGIN", unique("cid"), users[static_cast<std::size_t>(i)], "n", "0"));
    });
  }
  for (auto &t : threads) {
    t.join();
  }

  for (const auto &user : users) {
    REQUIRE(waitUntil(std::chrono::seconds(2), [&] { return db().isUserOnline(user); }));
  }

  fixture.gossip->stop();
}

// 22. typical LOGIN / MESSAGE / LOGOUT flow
TEST_CASE("GossipManager typical LOGIN MESSAGE LOGOUT flow", "[gossip_manager][flow]") {
  REQUIRE(winsock().ok);
  GossipFixture fixture;
  REQUIRE(fixture.start());

  const std::string user = unique("flow");
  const std::string email = user + "@example.com";
  REQUIRE_NOTHROW(db().createUser(user, "secret", email));

  fixture.gossip->rumor(makeEvent("LOGIN", unique("fl"), user, fixture.nodeId, "0"));
  REQUIRE(waitUntil(std::chrono::seconds(1), [&] { return db().isUserOnline(user); }));

  const std::string msgId = unique("fm");
  const std::string ts = std::to_string(static_cast<long long>(std::time(nullptr)));
  fixture.gossip->rumor(makeEvent("MESSAGE", msgId, user, "flow-hi", ts, "Lobby"));
  REQUIRE(waitUntil(std::chrono::seconds(2), [&] {
    for (const auto &m : db().loadHistory(1)) {
      if (m.getId() == msgId) {
        return true;
      }
    }
    return false;
  }));

  fixture.gossip->rumor(makeEvent("LOGOUT", unique("fo"), user, fixture.nodeId, "0"));
  REQUIRE(waitUntil(std::chrono::seconds(1), [&] { return !db().isUserOnline(user); }));

  fixture.gossip->stop();
}
