/**
 * PacketProcessor unit tests
 *
 * @brief Includes: heartbeat / gossip reject / default, register edges,
 * login success and failures, logout, message auth/empty/pipes, room join/leave,
 * response envelope, typical register→login→message→logout flow.
 * @date 13-09-2026
 */

#include "config/config.h"
#include "server/client_session.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/gossip_manager.h"
#include "server/packet_processor.h"
#include "server/room_manager.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <memory>
#include <random>
#include <string>
#include <string_view>

/**
 * 1. heartbeat pong
 * 2. gossip types rejected on client port
 * 3. unknown / DEFAULT packet type
 * 4. register missing fields
 * 5. register success and duplicate
 * 6. login wrong password / unknown user
 * 7. login success
 * 8. login rejected when already online
 * 9. logout anonymous and authenticated
 * 10. message requires auth / rejects empty
 * 11. message success including pipes
 * 12. room join / leave auth and unknown room
 * 13. room join empty room -> Lobby; leave -> Lobby
 * 14. response envelope fields
 * 15. typical register / message / logout flow
 */

namespace {

std::string unique(std::string_view prefix) {
  static std::atomic<std::uint64_t> seq{0};
  const auto n = seq.fetch_add(1);
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::string(prefix) + "_" + std::to_string(n) + "_" + std::to_string(now);
}

int nextFakeSocket() {
  static std::atomic<int> next{70000};
  return next.fetch_add(1);
}

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / "dcs-packet-processor-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / ("node-" + unique("pid") + "-" + std::to_string(rd()) + ".db");
    config::DB_PATH = path.string();
    config::NODE_ID = "pp-test-node";
    return &DatabaseManager::getInstance();
  }();
  return *instance;
}

RoomManager &rooms() { return RoomManager::getInstance(); }

Packet process(const Packet &packet, ClientSession &session, ConnectionManager &connections) {
  return PacketProcessor::processPacket(packet, session, connections);
}

struct Fixture {
  ConnectionManager connections;
  std::unique_ptr<GossipManager> gossip;
  ClientSession session;

  Fixture()
      : gossip(std::make_unique<GossipManager>(connections)),
        session(nextFakeSocket(), User::anonymousUser(), RoomManager::LOBBY) {
    (void)db();
    connections.setGossip(gossip.get());
  }

  Fixture(const Fixture &) = delete;
  Fixture &operator=(const Fixture &) = delete;

  ~Fixture() {
    const std::string username = session.getUser().getUsername();
    rooms().leaveAll(session);
    connections.setGossip(nullptr);
    if (!username.empty()) {
      try {
        db().clearOnline(username);
        db().clearAllMembership(username);
      } catch (...) {
      }
    }
  }

  User createUser(std::string_view password = "secret") {
    const std::string name = unique("user");
    return db().createUser(name, password, name + "@example.com");
  }

  void authenticate(const User &user) {
    session.setUser(user);
    session.setAuthenticated(true);
    REQUIRE(rooms().joinRoom(RoomManager::LOBBY.getName(), session));
  }
};

} // namespace

// 1. heartbeat pong
TEST_CASE("PacketProcessor heartbeat returns pong", "[packet_processor][heartbeat]") {
  Fixture fx;
  Packet req("client", "server", Packet::PacketType::HEARTBEAT, "", "ping");
  const Packet res = process(req, fx.session, fx.connections);
  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(res.message == "pong");
  REQUIRE(res.sender == "server");
  REQUIRE(res.receiver == "client");
}

// 2. gossip types rejected on client port
TEST_CASE("PacketProcessor rejects gossip on client port", "[packet_processor][gossip][edge]") {
  Fixture fx;
  const Packet::PacketType types[] = {
      Packet::PacketType::GOSSIP_HELLO, Packet::PacketType::GOSSIP_EVENT,
      Packet::PacketType::GOSSIP_DIGEST, Packet::PacketType::GOSSIP_PULL};
  for (Packet::PacketType type : types) {
    Packet req("peer", "*", type, "", "x");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "unsupported on client port");
  }
}

// 3. unknown / DEFAULT packet type
TEST_CASE("PacketProcessor rejects unknown packet type", "[packet_processor][default][edge]") {
  Fixture fx;
  Packet req("alice", "server", Packet::PacketType::DEFAULT, "", "noop");
  const Packet res = process(req, fx.session, fx.connections);
  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
  REQUIRE(res.message == "unknown packet type");
}

// 4. register missing fields
TEST_CASE("PacketProcessor register rejects missing fields", "[packet_processor][register][edge]") {
  Fixture fx;

  SECTION("empty username") {
    Packet req("", "server", Packet::PacketType::REGISTER, "a@b.c", "pw");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "missing username, password, or email");
  }

  SECTION("empty password") {
    Packet req(unique("u"), "server", Packet::PacketType::REGISTER, "a@b.c", "");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "missing username, password, or email");
  }

  SECTION("empty email") {
    Packet req(unique("u"), "server", Packet::PacketType::REGISTER, "", "pw");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "missing username, password, or email");
  }
}

// 5. register success and duplicate
TEST_CASE("PacketProcessor register success and duplicate", "[packet_processor][register]") {
  Fixture fx;
  const std::string name = unique("reg");
  const std::string email = name + "@mail.test";

  Packet req(name, "server", Packet::PacketType::REGISTER, email, "secret");
  const Packet res = process(req, fx.session, fx.connections);
  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(fx.session.isAuthenticated());
  REQUIRE(fx.session.getUser().getUsername() == name);
  REQUIRE(db().userExists(name));
  REQUIRE(db().isUserOnline(name));
  REQUIRE(fx.session.getRoom().getName() == RoomManager::LOBBY.getName());
  REQUIRE_NOTHROW(User::deserialize(res.message));

  Packet again(name, "server", Packet::PacketType::REGISTER, email, "secret");
  ClientSession other(nextFakeSocket(), User::anonymousUser(), RoomManager::LOBBY);
  const Packet dup = process(again, other, fx.connections);
  REQUIRE(dup.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
  REQUIRE(dup.message == "user already exists");
  rooms().leaveAll(other);
}

// 6. login wrong password / unknown user
TEST_CASE("PacketProcessor login failures", "[packet_processor][login][edge]") {
  Fixture fx;
  const User user = fx.createUser("right");

  SECTION("wrong password") {
    Packet req(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "wrong");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE_FALSE(fx.session.isAuthenticated());
  }

  SECTION("unknown user") {
    Packet req(unique("nobody"), "server", Packet::PacketType::LOGIN, "", "pw");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
  }
}

// 7. login success
TEST_CASE("PacketProcessor login success", "[packet_processor][login]") {
  Fixture fx;
  const User user = fx.createUser("secret");
  Packet req(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "secret");
  const Packet res = process(req, fx.session, fx.connections);

  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(fx.session.isAuthenticated());
  REQUIRE(fx.session.getUser().getUsername() == user.getUsername());
  REQUIRE(db().isUserOnline(user.getUsername()));
  REQUIRE(fx.session.getRoom().getName() == RoomManager::LOBBY.getName());
  REQUIRE_NOTHROW(User::deserialize(res.message));
}

// 8. login rejected when already online
TEST_CASE("PacketProcessor login rejects already online", "[packet_processor][login][edge]") {
  Fixture fx;
  const User user = fx.createUser("secret");
  db().setOnline(user.getUsername(), "other-node");

  Packet req(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "secret");
  const Packet res = process(req, fx.session, fx.connections);
  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
  REQUIRE(res.message == "user already logged in");
  REQUIRE_FALSE(fx.session.isAuthenticated());

  db().clearOnline(user.getUsername());
}

// 9. logout anonymous and authenticated
TEST_CASE("PacketProcessor logout", "[packet_processor][logout]") {
  Fixture fx;

  SECTION("anonymous succeeds") {
    Packet req("", "server", Packet::PacketType::LOGOUT);
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(res.message == "logged out");
    REQUIRE_FALSE(fx.session.isAuthenticated());
  }

  SECTION("authenticated clears presence") {
    const User user = fx.createUser("secret");
    Packet login(user.getUsername(), "server", Packet::PacketType::LOGIN, "", "secret");
    REQUIRE(process(login, fx.session, fx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(db().isUserOnline(user.getUsername()));

    Packet logout(user.getUsername(), "server", Packet::PacketType::LOGOUT);
    const Packet res = process(logout, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE_FALSE(fx.session.isAuthenticated());
    REQUIRE_FALSE(db().isUserOnline(user.getUsername()));
    REQUIRE(fx.session.getRoom().getName() == RoomManager::LOBBY.getName());
  }
}

// 10. message requires auth / rejects empty
TEST_CASE("PacketProcessor message auth and empty", "[packet_processor][message][edge]") {
  Fixture fx;

  SECTION("not authenticated") {
    Packet req("alice", "server", Packet::PacketType::MESSAGE, "Lobby", "hi");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "not authenticated");
  }

  SECTION("empty message") {
    const User user = fx.createUser();
    fx.authenticate(user);
    Packet req(user.getUsername(), "server", Packet::PacketType::MESSAGE, "Lobby", "");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "empty message");
  }
}

// 11. message success including pipes
TEST_CASE("PacketProcessor message success with pipes", "[packet_processor][message]") {
  Fixture fx;
  const User user = fx.createUser();
  fx.authenticate(user);

  const std::string body = "hello|with|pipes";
  Packet req(user.getUsername(), "server", Packet::PacketType::MESSAGE, "Lobby", body);
  const Packet res = process(req, fx.session, fx.connections);
  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(res.message == "ok");

  bool found = false;
  for (const auto &m : db().loadHistory(RoomManager::LOBBY.getName())) {
    if (m.getContent() == body && m.getFrom().getUsername() == user.getUsername()) {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

// 12. room join / leave auth and unknown room
TEST_CASE("PacketProcessor room join leave edges", "[packet_processor][room][edge]") {
  Fixture fx;

  SECTION("join not authenticated") {
    Packet req("alice", "server", Packet::PacketType::ROOM_JOIN, "General", "");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "not authenticated");
  }

  SECTION("leave not authenticated") {
    Packet req("alice", "server", Packet::PacketType::ROOM_LEAVE, "General", "");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "not authenticated");
  }

  SECTION("unknown room") {
    const User user = fx.createUser();
    fx.authenticate(user);
    Packet req(user.getUsername(), "server", Packet::PacketType::ROOM_JOIN, "NoSuchRoom", "");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::NOT_FOUND));
    REQUIRE(res.message.find("unknown room") != std::string::npos);
  }
}

// 13. room join empty room -> Lobby; leave -> Lobby
TEST_CASE("PacketProcessor room join and leave", "[packet_processor][room]") {
  Fixture fx;
  const User user = fx.createUser();
  fx.authenticate(user);

  Packet toGeneral(user.getUsername(), "server", Packet::PacketType::ROOM_JOIN, "General", "");
  const Packet joined = process(toGeneral, fx.session, fx.connections);
  REQUIRE(joined.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(joined.room == "General");
  REQUIRE(fx.session.getRoom().getName() == "General");

  Packet emptyRoom(user.getUsername(), "server", Packet::PacketType::ROOM_JOIN, "", "");
  const Packet lobbyJoin = process(emptyRoom, fx.session, fx.connections);
  REQUIRE(lobbyJoin.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(lobbyJoin.room == RoomManager::LOBBY.getName());

  REQUIRE(process(toGeneral, fx.session, fx.connections).responseCode ==
          static_cast<int>(RESPONSE_CODES::SUCCESS));

  Packet leave(user.getUsername(), "server", Packet::PacketType::ROOM_LEAVE, "General", "");
  const Packet left = process(leave, fx.session, fx.connections);
  REQUIRE(left.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(left.room == RoomManager::LOBBY.getName());
  REQUIRE(fx.session.getRoom().getName() == RoomManager::LOBBY.getName());
}

// 14. response envelope fields
TEST_CASE("PacketProcessor response envelope", "[packet_processor][envelope]") {
  Fixture fx;
  Packet req("alice", "ignored", Packet::PacketType::HEARTBEAT, "r", "ping", 123);
  const auto before = static_cast<std::uint64_t>(std::time(nullptr));
  const Packet res = process(req, fx.session, fx.connections);
  const auto after = static_cast<std::uint64_t>(std::time(nullptr));

  REQUIRE(res.sender == "server");
  REQUIRE(res.receiver == "alice");
  REQUIRE(res.type == Packet::PacketType::HEARTBEAT);
  REQUIRE(res.timestamp >= before);
  REQUIRE(res.timestamp <= after);
}

// 15. typical register / message / logout flow
TEST_CASE("PacketProcessor typical register message logout flow", "[packet_processor][flow]") {
  Fixture fx;
  const std::string name = unique("flow");
  const std::string email = name + "@example.com";

  Packet reg(name, "server", Packet::PacketType::REGISTER, email, "pw");
  REQUIRE(process(reg, fx.session, fx.connections).responseCode ==
          static_cast<int>(RESPONSE_CODES::SUCCESS));

  Packet msg(name, "server", Packet::PacketType::MESSAGE, "Lobby", "flow-hi|ok");
  REQUIRE(process(msg, fx.session, fx.connections).responseCode ==
          static_cast<int>(RESPONSE_CODES::SUCCESS));

  bool found = false;
  for (const auto &m : db().loadHistory(RoomManager::LOBBY.getName())) {
    if (m.getContent() == "flow-hi|ok" && m.getFrom().getUsername() == name) {
      found = true;
      break;
    }
  }
  REQUIRE(found);

  Packet logout(name, "server", Packet::PacketType::LOGOUT);
  REQUIRE(process(logout, fx.session, fx.connections).responseCode ==
          static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE_FALSE(db().isUserOnline(name));
  REQUIRE_FALSE(fx.session.isAuthenticated());
}
