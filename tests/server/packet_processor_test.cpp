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
#include "utils/models/message.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
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
 * 10. message rejects empty; guests may send
 * 11. message success including pipes
 * 12. room join / leave: guest public OK, guest private denied, unknown room
 * 13. room join empty room -> Lobby; leave -> Lobby
 * 14. UPDATE_USER
 * 15. response envelope fields
 * 16. ROOM_CREATE
 * 17. LOAD_MESSAGE_HISTORY
 * 18. typical register / message / logout flow
 * 19. ADMIN privilege checks
 */

namespace {

std::string unique(std::string_view prefix) {
  static std::atomic<std::uint64_t> seq{0};
  const auto n = seq.fetch_add(1);
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::string(prefix) + "_" + std::to_string(n) + "_" + std::to_string(now);
}

SOCKET nextFakeSocket() {
  static std::atomic<SOCKET> next{70000};
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

  User createAdmin(std::string_view password = "secret") {
    User user = createUser(password);
    user.setUserType(User::UserType::ADMIN);
    return user;
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
  REQUIRE_FALSE(res.room.empty());
  REQUIRE_FALSE(Room::deserializeList(res.room).empty());

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
  REQUIRE_FALSE(res.room.empty());
  REQUIRE_NOTHROW(Room::deserializeList(res.room));
  REQUIRE_FALSE(Room::deserializeList(res.room).empty());
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

// 10. message: guests allowed; empty rejected
TEST_CASE("PacketProcessor message auth and empty", "[packet_processor][message][edge]") {
  Fixture fx;

  SECTION("guest can send") {
    REQUIRE_FALSE(fx.session.isAuthenticated());
    REQUIRE(rooms().joinRoom(RoomManager::LOBBY.getName(), fx.session));
    Packet req(fx.session.getUser().getUsername(), "server", Packet::PacketType::MESSAGE, "Lobby",
               "hi from guest");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(res.message == "ok");
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
  for (const auto &m : db().loadHistory(RoomManager::LOBBY.getId())) {
    if (m.getContent() == body && m.getFrom().getUsername() == user.getUsername()) {
      found = true;
      break;
    }
  }
  REQUIRE(found);
}

// 12. room join / leave: guests on public, private denied, unknown room
TEST_CASE("PacketProcessor room join leave edges", "[packet_processor][room][edge]") {
  Fixture fx;

  SECTION("guest can join public room") {
    REQUIRE_FALSE(fx.session.isAuthenticated());
    Packet req(fx.session.getUser().getUsername(), "server", Packet::PacketType::ROOM_JOIN,
               "General", "");
    const Packet res = process(req, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(res.room == "General");
    REQUIRE(fx.session.getRoom().getName() == "General");
  }

  SECTION("guest can leave to Lobby") {
    Packet join(fx.session.getUser().getUsername(), "server", Packet::PacketType::ROOM_JOIN,
                "General", "");
    REQUIRE(process(join, fx.session, fx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));

    Packet leave(fx.session.getUser().getUsername(), "server", Packet::PacketType::ROOM_LEAVE,
                 "General", "");
    const Packet res = process(leave, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(res.room == RoomManager::LOBBY.getName());
    REQUIRE(fx.session.getRoom().getName() == RoomManager::LOBBY.getName());
  }

  SECTION("guest denied private room") {
    const User owner = fx.createUser();
    fx.authenticate(owner);
    const std::string privateName = unique("guestvault");
    Packet create(owner.getUsername(), "server", Packet::PacketType::ROOM_CREATE, privateName,
                  "PRIVATE");
    REQUIRE(process(create, fx.session, fx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));

    Fixture guestFx;
    Packet join(guestFx.session.getUser().getUsername(), "server", Packet::PacketType::ROOM_JOIN,
                privateName, "");
    const Packet res = process(join, guestFx.session, guestFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message.find("not allowed") != std::string::npos);
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

// 14. UPDATE_USER password and username
TEST_CASE("PacketProcessor UPDATE_USER", "[packet_processor][update]") {
  Fixture fx;
  const std::string name = unique("upd");
  const std::string email = name + "@example.com";

  Packet reg(name, "server", Packet::PacketType::REGISTER, email, "oldpw");
  REQUIRE(process(reg, fx.session, fx.connections).responseCode ==
          static_cast<int>(RESPONSE_CODES::SUCCESS));

  SECTION("password change") {
    Packet upd(name, "server", Packet::PacketType::UPDATE_USER, email, "newpw");
    const Packet res = process(upd, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(fx.session.getUser().getUsername() == name);

    const auto login = db().loginUser(name, "newpw");
    REQUIRE(std::holds_alternative<User>(login));
  }

  SECTION("username rename") {
    const std::string renamed = unique("ren");
    Packet upd(renamed, "server", Packet::PacketType::UPDATE_USER, email, "");
    const Packet res = process(upd, fx.session, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(fx.session.getUser().getUsername() == renamed);

    const User restored = User::deserialize(res.message);
    REQUIRE(restored.getUsername() == renamed);
    REQUIRE(restored.getEmail() == email);
  }

  SECTION("rejects when not authenticated") {
    ClientSession anon(nextFakeSocket(), User::anonymousUser(), RoomManager::LOBBY);
    Packet upd(name, "server", Packet::PacketType::UPDATE_USER, email, "pw");
    const Packet res = process(upd, anon, fx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message == "not authenticated");
  }
}

// 15. response envelope fields
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

// 16. ROOM_CREATE creates room and rejects unauthenticated / duplicate
TEST_CASE("PacketProcessor ROOM_CREATE", "[packet_processor][create]") {
  Fixture fx;
  const User user = fx.createUser("secret");
  fx.authenticate(user);

  const std::string roomName = unique("room");
  Packet req(user.getUsername(), "server", Packet::PacketType::ROOM_CREATE, roomName, "");
  const Packet res = process(req, fx.session, fx.connections);

  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(res.room == roomName);
  REQUIRE(RoomManager::getInstance().getRoom(roomName).has_value());
  REQUIRE_NOTHROW(Room::deserialize(res.message));

  SECTION("private privacy") {
    const std::string privateName = unique("private");
    Packet privateReq(user.getUsername(), "server", Packet::PacketType::ROOM_CREATE, privateName,
                      "PRIVATE");
    const Packet privateRes = process(privateReq, fx.session, fx.connections);
    REQUIRE(privateRes.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    const auto created = RoomManager::getInstance().getRoom(privateName);
    REQUIRE(created.has_value());
    REQUIRE(created->getPrivacy() == Room::Privacy::PRIVATE);
    REQUIRE(created->getType() == Room::RoomType::OTHER);
  }

  SECTION("duplicate name fails") {
    Packet again(user.getUsername(), "server", Packet::PacketType::ROOM_CREATE, roomName, "");
    const Packet dup = process(again, fx.session, fx.connections);
    REQUIRE(dup.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
  }

  SECTION("rejects when not authenticated") {
    ClientSession anon(nextFakeSocket(), User::anonymousUser(), RoomManager::LOBBY);
    Packet create("anon", "server", Packet::PacketType::ROOM_CREATE, unique("x"), "");
    const Packet denied = process(create, anon, fx.connections);
    REQUIRE(denied.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(denied.message == "not authenticated");
  }

  SECTION("ROOM_LIST on client port rejected") {
    Packet list(user.getUsername(), "server", Packet::PacketType::ROOM_LIST, "", "");
    const Packet denied = process(list, fx.session, fx.connections);
    REQUIRE(denied.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
  }

  SECTION("private room: stranger denied until invited") {
    const std::string privateName = unique("vault");
    Packet privateReq(user.getUsername(), "server", Packet::PacketType::ROOM_CREATE, privateName,
                      "PRIVATE");
    REQUIRE(process(privateReq, fx.session, fx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));

    Fixture other;
    const User guest = other.createUser("secret");
    other.authenticate(guest);

    Packet join(guest.getUsername(), "server", Packet::PacketType::ROOM_JOIN, privateName, "");
    const Packet denied = process(join, other.session, other.connections);
    REQUIRE(denied.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(denied.message.find("not allowed") != std::string::npos);

    Packet invite(user.getUsername(), "server", Packet::PacketType::ROOM_INVITE, privateName,
                  guest.getUsername());
    REQUIRE(process(invite, fx.session, fx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));

    const Packet joined = process(join, other.session, other.connections);
    REQUIRE(joined.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  }
}

// 17. LOAD_MESSAGE_HISTORY returns encoded history
TEST_CASE("PacketProcessor LOAD_MESSAGE_HISTORY", "[packet_processor][history]") {
  Fixture fx;
  const User user = fx.createUser("secret");
  fx.authenticate(user);

  const Message msg(user, User::anonymousUser(), "hello history");
  REQUIRE(db().saveMessage(msg, RoomManager::LOBBY.getId()));

  Packet req(user.getUsername(), "server", Packet::PacketType::LOAD_MESSAGE_HISTORY, "Lobby", "");
  const Packet res = process(req, fx.session, fx.connections);

  REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
  REQUIRE(res.room == "Lobby");
  REQUIRE(res.message.find("[") != std::string::npos);
  REQUIRE(res.message.find(user.getUsername()) != std::string::npos);
  REQUIRE(res.message.find("hello history") != std::string::npos);

  SECTION("guest can load history") {
    ClientSession anon(nextFakeSocket(), User::anonymousUser(), RoomManager::LOBBY);
    Packet history(anon.getUser().getUsername(), "server",
                   Packet::PacketType::LOAD_MESSAGE_HISTORY, "Lobby", "");
    const Packet allowed = process(history, anon, fx.connections);
    REQUIRE(allowed.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(allowed.message.find("hello history") != std::string::npos);
  }

  SECTION("unknown room returns NOT_FOUND") {
    Packet history(user.getUsername(), "server", Packet::PacketType::LOAD_MESSAGE_HISTORY,
                   unique("missing"), "");
    const Packet denied = process(history, fx.session, fx.connections);
    REQUIRE(denied.responseCode == static_cast<int>(RESPONSE_CODES::NOT_FOUND));
  }
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
  for (const auto &m : db().loadHistory(RoomManager::LOBBY.getId())) {
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

// 18. ADMIN privilege checks
TEST_CASE("PacketProcessor ADMIN privilege checks", "[packet_processor][admin]") {
  Fixture ownerFx;
  const User owner = ownerFx.createUser();
  ownerFx.authenticate(owner);

  const std::string privateName = unique("vault");
  Packet create(owner.getUsername(), "server", Packet::PacketType::ROOM_CREATE, privateName,
                "PRIVATE");
  REQUIRE(process(create, ownerFx.session, ownerFx.connections).responseCode ==
          static_cast<int>(RESPONSE_CODES::SUCCESS));
  const auto room = RoomManager::getInstance().getRoom(privateName);
  REQUIRE(room.has_value());

  Fixture adminFx;
  const User admin = adminFx.createAdmin();
  adminFx.authenticate(admin);

  Fixture userFx;
  const User stranger = userFx.createUser();
  userFx.authenticate(stranger);

  SECTION("admin joins private without allow_list") {
    Packet join(admin.getUsername(), "server", Packet::PacketType::ROOM_JOIN, privateName, "");
    const Packet res = process(join, adminFx.session, adminFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(adminFx.session.getRoom().getName() == privateName);
  }

  SECTION("user still denied private without invite") {
    Packet join(stranger.getUsername(), "server", Packet::PacketType::ROOM_JOIN, privateName, "");
    const Packet res = process(join, userFx.session, userFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(res.message.find("not allowed") != std::string::npos);
  }

  SECTION("admin invites without being creator") {
    Packet invite(admin.getUsername(), "server", Packet::PacketType::ROOM_INVITE, privateName,
                  stranger.getUsername());
    const Packet res = process(invite, adminFx.session, adminFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(db().isAllowed(room->getId(), stranger.getEmail()));

    Packet join(stranger.getUsername(), "server", Packet::PacketType::ROOM_JOIN, privateName, "");
    REQUIRE(process(join, userFx.session, userFx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));
  }

  SECTION("user invite without being creator is forbidden") {
    Packet invite(stranger.getUsername(), "server", Packet::PacketType::ROOM_INVITE, privateName,
                  admin.getUsername());
    const Packet res = process(invite, userFx.session, userFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::FORBIDDEN));
  }

  SECTION("admin kicks without being creator") {
    Packet invite(owner.getUsername(), "server", Packet::PacketType::ROOM_INVITE, privateName,
                  stranger.getUsername());
    REQUIRE(process(invite, ownerFx.session, ownerFx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE(db().isAllowed(room->getId(), stranger.getEmail()));

    Packet kick(admin.getUsername(), "server", Packet::PacketType::ROOM_KICK, privateName,
                stranger.getUsername());
    const Packet res = process(kick, adminFx.session, adminFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE_FALSE(db().isAllowed(room->getId(), stranger.getEmail()));
  }

  SECTION("user kick without being creator is forbidden") {
    Packet kick(stranger.getUsername(), "server", Packet::PacketType::ROOM_KICK, privateName,
                owner.getUsername());
    const Packet res = process(kick, userFx.session, userFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::FORBIDDEN));
  }

  SECTION("creator can kick") {
    Packet invite(owner.getUsername(), "server", Packet::PacketType::ROOM_INVITE, privateName,
                  stranger.getUsername());
    REQUIRE(process(invite, ownerFx.session, ownerFx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::SUCCESS));

    Packet kick(owner.getUsername(), "server", Packet::PacketType::ROOM_KICK, privateName,
                stranger.getUsername());
    const Packet res = process(kick, ownerFx.session, ownerFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE_FALSE(db().isAllowed(room->getId(), stranger.getEmail()));
  }

  SECTION("admin deletes a private room") {
    Packet del(admin.getUsername(), "server", Packet::PacketType::ROOM_DELETE, privateName, "");
    const Packet res = process(del, adminFx.session, adminFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::SUCCESS));
    REQUIRE_FALSE(RoomManager::getInstance().getRoom(privateName).has_value());
  }

  SECTION("user cannot delete a room") {
    Packet del(stranger.getUsername(), "server", Packet::PacketType::ROOM_DELETE, privateName, "");
    const Packet res = process(del, userFx.session, userFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::FORBIDDEN));
    REQUIRE(RoomManager::getInstance().getRoom(privateName).has_value());
  }

  SECTION("admin cannot delete Lobby or General") {
    Packet lobby(admin.getUsername(), "server", Packet::PacketType::ROOM_DELETE, "Lobby", "");
    REQUIRE(process(lobby, adminFx.session, adminFx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::ERROR));
    Packet general(admin.getUsername(), "server", Packet::PacketType::ROOM_DELETE, "General", "");
    REQUIRE(process(general, adminFx.session, adminFx.connections).responseCode ==
            static_cast<int>(RESPONSE_CODES::ERROR));
    REQUIRE(RoomManager::getInstance().getRoom("Lobby").has_value());
    REQUIRE(RoomManager::getInstance().getRoom("General").has_value());
  }

  SECTION("anonymous cannot delete") {
    ClientSession anon(nextFakeSocket(), User::anonymousUser(), RoomManager::LOBBY);
    Packet del("anon", "server", Packet::PacketType::ROOM_DELETE, privateName, "");
    const Packet res = process(del, anon, ownerFx.connections);
    REQUIRE(res.responseCode == static_cast<int>(RESPONSE_CODES::FORBIDDEN));
  }
}
