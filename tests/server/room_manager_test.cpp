/**
 * RoomManager unit tests
 *
 * @brief Includes: getRoom edges, create/delete room edges, join/leave edges,
 * auth persist + leaveAll teardown, broadcast / broadcastAll / skipSocket,
 * deleteRoom moves members to Lobby, concurrent joins, typical room flow.
 * @date 13-09-2026
 */

#include "auth/authentication.h"
#include "config/config.h"
#include "server/client_session.h"
#include "server/connection_manager.h"
#include "server/database_manager.h"
#include "server/room_manager.h"
#include "utils/models/packet.h"
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
#include <unordered_set>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

/**
 * 1. singleton identity
 * 2. getRoom edges
 * 3. createRoom edges
 * 4. deleteRoom refuses Lobby / unknown
 * 5. joinRoom unknown / empty name
 * 6. joinRoom moves between rooms
 * 7. joinRoom same room
 * 8. leaveAll teardown (no Lobby member)
 * 9. authenticated join + leaveAll clears DB membership
 * 10. broadcast empty room
 * 11. broadcast delivers and skipSocket
 * 12. broadcastAll fans out
 * 13. deleteRoom moves live sessions to Lobby
 * 14. concurrent joins
 * 15. typical create / join / leave / delete flow
 * 16. listRooms includes defaults and created rooms
 * 17. leaveAll is idempotent for Lobby-only session
 * 18. broadcastAll respects skipSocket
 */

namespace {

constexpr auto kAcceptWait = std::chrono::seconds(2);

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

SOCKET nextFakeSocket() {
  static std::atomic<SOCKET> next{60000};
  return next.fetch_add(1);
}

DatabaseManager &db() {
  static DatabaseManager *instance = []() -> DatabaseManager * {
    std::random_device rd;
    const auto dir = std::filesystem::temp_directory_path() / "dcs-room-manager-tests";
    std::filesystem::create_directories(dir);
    const auto path = dir / ("node-" + unique("pid") + "-" + std::to_string(rd()) + ".db");
    config::DB_PATH = path.string();
    return &DatabaseManager::getInstance();
  }();
  return *instance;
}

// Always open the isolated test DB before constructing RoomManager (loads rooms from DB).
RoomManager &rooms() {
  db();
  return RoomManager::getInstance();
}

// Extra seeded-style room used by join/broadcast tests (not in init.sql)
void ensureRandomRoom() {
  static const bool ready = [] {
    rooms().createRoom(Room(0, "Random"));
    return true;
  }();
  (void)ready;
}

User makeUser(std::string_view name) {
  return User(name, std::string(name) + "@example.com", User::UserType::USER);
}

Packet makeMsg(std::string_view body, std::string_view room = "") {
  return Packet("server", "all", Packet::PacketType::MESSAGE, room, body, 0);
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

bool setRecvTimeout(SOCKET socket, DWORD milliseconds) {
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&milliseconds),
                    sizeof(milliseconds)) == 0;
}

// RAII: leave room membership on destruction (singleton cleanup)
struct TrackedSession {
  ClientSession session;

  explicit TrackedSession(SOCKET socket, const User &user = User::anonymousUser(),
                          const Room &room = RoomManager::LOBBY)
      : session(socket, user, room) {}

  TrackedSession(const TrackedSession &) = delete;
  TrackedSession &operator=(const TrackedSession &) = delete;

  ~TrackedSession() { rooms().leaveAll(session); }

  ClientSession &get() { return session; }
};

struct NetFixture {
  struct ConnectedClient {
    SOCKET client{INVALID_SOCKET};
    std::shared_ptr<ClientSession> session;
  };

  ConnectionManager connections;
  std::thread acceptThread;
  std::vector<SOCKET> clients;

  NetFixture() = default;
  NetFixture(const NetFixture &) = delete;
  NetFixture &operator=(const NetFixture &) = delete;

  ~NetFixture() {
    // Wake accept + client reader threads, then wait until handleClient has removed
    // every session. Those threads are detached; destroying ConnectionManager while
    // they still run causes a use-after-free / segfault after Catch reports pass.
    connections.stopListening();
    for (SOCKET socket : clients) {
      if (socket != INVALID_SOCKET) {
        socket_io::close(socket);
      }
    }
    waitUntil(kAcceptWait, [&] { return connections.getSessions().empty(); });
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

  // sessions are keyed by the server accept fd, not the client SOCKET
  ConnectedClient connectClient() {
    ConnectedClient out;
    std::unordered_set<SOCKET> known;
    for (const auto &entry : connections.getSessions()) {
      known.insert(entry.first);
    }

    const SOCKET listenFd = connections.getListeningSocket();
    sockaddr_in bound{};
    int boundLen = sizeof(bound);
    if (getsockname(listenFd, reinterpret_cast<sockaddr *>(&bound), &boundLen) != 0) {
      return out;
    }

    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == INVALID_SOCKET) {
      return out;
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = bound.sin_port;
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(client, reinterpret_cast<sockaddr *>(&dest), sizeof(dest)) != 0) {
      socket_io::close(client);
      return out;
    }

    clients.push_back(client);
    const std::size_t expected = clients.size();
    if (!waitUntil(kAcceptWait, [&] { return connections.getSessions().size() >= expected; })) {
      return out;
    }

    for (const auto &entry : connections.getSessions()) {
      if (known.find(entry.first) == known.end()) {
        out.client = client;
        out.session = entry.second;
        // drain connect welcome (ROOM_LIST) so broadcast asserts see only MESSAGE
        setRecvTimeout(client, 2000);
        const auto welcome = socket_io::readPacket(client);
        if (!welcome || welcome->type != Packet::PacketType::ROOM_LIST) {
          out.client = INVALID_SOCKET;
          out.session.reset();
        }
        return out;
      }
    }
    return out;
  }
};

} // namespace

// 1. singleton identity
TEST_CASE("RoomManager singleton identity", "[room_manager][singleton]") {
  // Call rooms() first so db() sets an isolated DB_PATH before RoomManager/DatabaseManager
  // construct. Evaluating getInstance() first would open the default data/*.db (often stale).
  RoomManager &viaHelper = rooms();
  REQUIRE(&viaHelper == &RoomManager::getInstance());
}

// 2. getRoom edges
TEST_CASE("RoomManager getRoom edges", "[room_manager][getRoom][edge]") {
  auto lobby = rooms().getRoom("");
  REQUIRE(lobby);
  REQUIRE(lobby->getName() == RoomManager::LOBBY.getName());

  auto byName = rooms().getRoom(RoomManager::LOBBY.getName());
  REQUIRE(byName);
  REQUIRE(byName->getName() == RoomManager::LOBBY.getName());

  REQUIRE(rooms().getRoom("General"));
  REQUIRE_FALSE(rooms().getRoom(unique("missing")));
  REQUIRE_FALSE(rooms().getRoom("general")); // case-sensitive
}

// 3. createRoom edges
TEST_CASE("RoomManager createRoom edges", "[room_manager][createRoom][edge]") {
  const std::string name = unique("room");
  Room room(0, name);

  REQUIRE(rooms().createRoom(room));
  REQUIRE_FALSE(rooms().createRoom(room));
  REQUIRE_FALSE(rooms().createRoom(Room(0, name)));
  REQUIRE_FALSE(rooms().createRoom(RoomManager::LOBBY));
  REQUIRE(rooms().getRoom(name));
  REQUIRE(rooms().getRoom(name)->getId() > 0);

  const std::string privName = unique("priv");
  REQUIRE(rooms().createRoom(
      Room(0, privName, Room::RoomType::SECURITY, Room::Privacy::PRIVATE)));
  auto priv = rooms().getRoom(privName);
  REQUIRE(priv);
  REQUIRE(priv->getPrivacy() == Room::Privacy::PRIVATE);

  ConnectionManager connections;
  REQUIRE(rooms().deleteRoom(name, connections));
  REQUIRE(rooms().deleteRoom(privName, connections));
  REQUIRE_FALSE(rooms().getRoom(name));
  REQUIRE_FALSE(rooms().getRoom(privName));
}

// 4. deleteRoom refuses Lobby / unknown
TEST_CASE("RoomManager deleteRoom refuses Lobby and unknown", "[room_manager][deleteRoom][edge]") {
  ConnectionManager connections;

  REQUIRE_FALSE(rooms().deleteRoom(RoomManager::LOBBY.getName(), connections));
  REQUIRE_FALSE(rooms().deleteRoom(unique("gone"), connections));
  REQUIRE(rooms().getRoom(RoomManager::LOBBY.getName()));
}

// 5. joinRoom unknown / empty name
TEST_CASE("RoomManager joinRoom unknown and empty name", "[room_manager][joinRoom][edge]") {
  TrackedSession tracked(nextFakeSocket());
  ClientSession &session = tracked.get();
  const Room before = session.getRoom();

  REQUIRE_FALSE(rooms().joinRoom(unique("nope"), session));
  REQUIRE(session.getRoom().getName() == before.getName());

  REQUIRE(rooms().joinRoom("", session));
  REQUIRE(session.getRoom().getName() == RoomManager::LOBBY.getName());
}

// 6. joinRoom moves between rooms
TEST_CASE("RoomManager joinRoom moves between rooms", "[room_manager][joinRoom]") {
  ensureRandomRoom();
  TrackedSession a(nextFakeSocket());
  TrackedSession b(nextFakeSocket());

  REQUIRE(rooms().joinRoom("General", a.get()));
  REQUIRE(a.get().getRoom().getName() == "General");

  REQUIRE(rooms().joinRoom("Random", a.get()));
  REQUIRE(a.get().getRoom().getName() == "Random");

  REQUIRE(rooms().joinRoom("General", b.get()));
  REQUIRE(b.get().getRoom().getName() == "General");

  // a left General: only b remains for General broadcasts with no ConnectionManager sessions
  ConnectionManager connections;
  Packet packet = makeMsg("ping-general", "General");
  auto general = rooms().getRoom("General");
  REQUIRE(general);
  // send fails (no live sessions) but membership path still runs
  REQUIRE_FALSE(rooms().broadcast(*general, packet, connections));
}

// 7. joinRoom same room
TEST_CASE("RoomManager joinRoom same room is idempotent", "[room_manager][joinRoom][edge]") {
  TrackedSession tracked(nextFakeSocket());
  REQUIRE(rooms().joinRoom("General", tracked.get()));
  REQUIRE(rooms().joinRoom("General", tracked.get()));
  REQUIRE(tracked.get().getRoom().getName() == "General");
}

// 8. leaveAll teardown (no Lobby member)
TEST_CASE("RoomManager leaveAll removes membership without Lobby insert",
          "[room_manager][leaveAll][edge]") {
  REQUIRE(winsock().ok);
  NetFixture net;
  REQUIRE(net.listen());

  auto connected = net.connectClient();
  REQUIRE(connected.client != INVALID_SOCKET);
  REQUIRE(connected.session);

  REQUIRE(rooms().joinRoom("General", *connected.session));
  REQUIRE(connected.session->getRoom().getName() == "General");

  rooms().leaveAll(*connected.session);
  REQUIRE(connected.session->getRoom().getName() == RoomManager::LOBBY.getName());

  // not in Lobby members: broadcastAll should not deliver to this client
  REQUIRE(setRecvTimeout(connected.client, 200));
  Packet packet = makeMsg("after-leave", RoomManager::LOBBY.getName());
  rooms().broadcastAll(packet, net.connections);

  auto received = socket_io::readPacket(connected.client);
  REQUIRE_FALSE(received.has_value());
}

// 9. authenticated join + leaveAll clears DB membership
TEST_CASE("RoomManager authenticated join persists and leaveAll clears",
          "[room_manager][joinRoom][leaveAll][db]") {
  db(); // ensure isolated DB before first persist
  ensureRandomRoom();

  const std::string name = unique("auth");
  REQUIRE_NOTHROW(db().createUser(name, Authentication::hashPassword("secret"),
                                  name + "@example.com"));
  TrackedSession tracked(nextFakeSocket(), makeUser(name));
  tracked.get().setAuthenticated(true);

  REQUIRE(rooms().joinRoom("General", tracked.get()));
  REQUIRE(tracked.get().getRoom().getName() == "General");

  REQUIRE(rooms().joinRoom("Random", tracked.get()));
  REQUIRE(tracked.get().getRoom().getName() == "Random");

  rooms().leaveAll(tracked.get());
  REQUIRE(tracked.get().getRoom().getName() == RoomManager::LOBBY.getName());
  REQUIRE_NOTHROW(db().clearAllMembership(name));
}

// 10. broadcast empty room
TEST_CASE("RoomManager broadcast empty room succeeds", "[room_manager][broadcast][edge]") {
  ConnectionManager connections;
  const std::string name = unique("empty");
  REQUIRE(rooms().createRoom(Room(0, name)));

  auto room = rooms().getRoom(name);
  REQUIRE(room);
  REQUIRE(rooms().broadcast(*room, makeMsg("nobody", name), connections));

  REQUIRE(rooms().deleteRoom(name, connections));
}

// 11. broadcast delivers and skipSocket
TEST_CASE("RoomManager broadcast delivers and respects skipSocket",
          "[room_manager][broadcast][edge]") {
  REQUIRE(winsock().ok);
  NetFixture net;
  REQUIRE(net.listen());

  auto c1 = net.connectClient();
  auto c2 = net.connectClient();
  REQUIRE(c1.client != INVALID_SOCKET);
  REQUIRE(c2.client != INVALID_SOCKET);
  REQUIRE(c1.session);
  REQUIRE(c2.session);

  const std::string name = unique("bcast");
  REQUIRE(rooms().createRoom(Room(0, name)));
  REQUIRE(rooms().joinRoom(name, *c1.session));
  REQUIRE(rooms().joinRoom(name, *c2.session));

  REQUIRE(setRecvTimeout(c1.client, 500));
  REQUIRE(setRecvTimeout(c2.client, 500));

  auto room = rooms().getRoom(name);
  REQUIRE(room);

  Packet packet = makeMsg("hello-room", name);
  REQUIRE(rooms().broadcast(*room, packet, net.connections, c1.session->getSocket()));

  auto skipped = socket_io::readPacket(c1.client);
  REQUIRE_FALSE(skipped.has_value());

  auto got = socket_io::readPacket(c2.client);
  REQUIRE(got);
  REQUIRE(got->message == "hello-room");

  rooms().leaveAll(*c1.session);
  rooms().leaveAll(*c2.session);
  REQUIRE(rooms().deleteRoom(name, net.connections));
}

// 12. broadcastAll fans out
TEST_CASE("RoomManager broadcastAll fans out across rooms", "[room_manager][broadcastAll]") {
  REQUIRE(winsock().ok);
  ensureRandomRoom();
  NetFixture net;
  REQUIRE(net.listen());

  auto c1 = net.connectClient();
  auto c2 = net.connectClient();
  REQUIRE(c1.client != INVALID_SOCKET);
  REQUIRE(c2.client != INVALID_SOCKET);
  REQUIRE(c1.session);
  REQUIRE(c2.session);

  REQUIRE(rooms().joinRoom("General", *c1.session));
  REQUIRE(rooms().joinRoom("Random", *c2.session));
  REQUIRE(setRecvTimeout(c1.client, 500));
  REQUIRE(setRecvTimeout(c2.client, 500));

  Packet packet = makeMsg("all-rooms");
  REQUIRE(rooms().broadcastAll(packet, net.connections));

  auto g1 = socket_io::readPacket(c1.client);
  auto g2 = socket_io::readPacket(c2.client);
  REQUIRE(g1);
  REQUIRE(g2);
  REQUIRE(g1->message == "all-rooms");
  REQUIRE(g2->message == "all-rooms");

  rooms().leaveAll(*c1.session);
  rooms().leaveAll(*c2.session);
}

// 13. deleteRoom moves live sessions to Lobby
TEST_CASE("RoomManager deleteRoom moves members to Lobby", "[room_manager][deleteRoom]") {
  REQUIRE(winsock().ok);
  NetFixture net;
  REQUIRE(net.listen());

  auto connected = net.connectClient();
  REQUIRE(connected.client != INVALID_SOCKET);
  REQUIRE(connected.session);

  db();
  const std::string user = unique("del");
  REQUIRE_NOTHROW(db().createUser(user, Authentication::hashPassword("secret"),
                                  user + "@example.com"));
  connected.session->setUser(makeUser(user));
  connected.session->setAuthenticated(true);

  const std::string name = unique("doomed");
  REQUIRE(rooms().createRoom(Room(0, name)));
  REQUIRE(rooms().joinRoom(name, *connected.session));
  REQUIRE(connected.session->getRoom().getName() == name);

  REQUIRE(rooms().deleteRoom(name, net.connections));
  REQUIRE_FALSE(rooms().getRoom(name));
  REQUIRE(connected.session->getRoom().getName() == RoomManager::LOBBY.getName());

  // deleteRoom pushes ROOM_LIST for the Lobby move — drain it before chat
  REQUIRE(setRecvTimeout(connected.client, 500));
  auto lobbyList = socket_io::readPacket(connected.client);
  REQUIRE(lobbyList);
  REQUIRE(lobbyList->type == Packet::PacketType::ROOM_LIST);
  REQUIRE(lobbyList->room == RoomManager::LOBBY.getName());

  // still in Lobby members after forced move
  REQUIRE(rooms().broadcast(RoomManager::LOBBY, makeMsg("lobby-hi", RoomManager::LOBBY.getName()),
                            net.connections));
  auto got = socket_io::readPacket(connected.client);
  REQUIRE(got);
  REQUIRE(got->message == "lobby-hi");

  rooms().leaveAll(*connected.session);
}

// 14. concurrent joins
TEST_CASE("RoomManager concurrent joins are safe", "[room_manager][joinRoom][concurrent]") {
  ensureRandomRoom();
  constexpr int kThreads = 8;
  std::vector<std::unique_ptr<TrackedSession>> sessions;
  sessions.reserve(kThreads);
  for (int i = 0; i < kThreads; ++i) {
    sessions.push_back(std::make_unique<TrackedSession>(nextFakeSocket(), makeUser(unique("c"))));
  }

  std::vector<std::thread> threads;
  threads.reserve(kThreads);
  std::atomic<int> ok{0};

  for (int i = 0; i < kThreads; ++i) {
    threads.emplace_back([&, i] {
      const char *room = (i % 2 == 0) ? "General" : "Random";
      if (rooms().joinRoom(room, sessions[static_cast<std::size_t>(i)]->get())) {
        ok.fetch_add(1);
      }
    });
  }
  for (auto &t : threads) {
    t.join();
  }

  REQUIRE(ok.load() == kThreads);
  for (auto &s : sessions) {
    const std::string &room = s->get().getRoom().getName();
    REQUIRE((room == "General" || room == "Random"));
  }
}

// 15. typical create / join / leave / delete flow
TEST_CASE("RoomManager typical create join leave delete flow", "[room_manager][flow]") {
  REQUIRE(winsock().ok);
  NetFixture net;
  REQUIRE(net.listen());

  auto connected = net.connectClient();
  REQUIRE(connected.client != INVALID_SOCKET);
  REQUIRE(connected.session);

  db();
  const std::string user = unique("flow");
  REQUIRE_NOTHROW(db().createUser(user, Authentication::hashPassword("secret"),
                                  user + "@example.com"));
  connected.session->setUser(makeUser(user));
  connected.session->setAuthenticated(true);

  const std::string name = unique("flowroom");
  REQUIRE(rooms().createRoom(Room(0, name, Room::RoomType::OTHER, Room::Privacy::PUBLIC)));
  REQUIRE(rooms().joinRoom(name, *connected.session));
  REQUIRE(connected.session->getRoom().getName() == name);

  REQUIRE(setRecvTimeout(connected.client, 500));
  auto room = rooms().getRoom(name);
  REQUIRE(room);
  REQUIRE(rooms().broadcast(*room, makeMsg("in-room", name), net.connections));
  auto got = socket_io::readPacket(connected.client);
  REQUIRE(got);
  REQUIRE(got->message == "in-room");

  REQUIRE(rooms().joinRoom(RoomManager::LOBBY.getName(), *connected.session));
  REQUIRE(connected.session->getRoom().getName() == RoomManager::LOBBY.getName());

  REQUIRE(rooms().deleteRoom(name, net.connections));
  REQUIRE_FALSE(rooms().getRoom(name));

  rooms().leaveAll(*connected.session);
  REQUIRE(connected.session->getRoom().getName() == RoomManager::LOBBY.getName());
}

// 16. listRooms includes defaults and created rooms
TEST_CASE("RoomManager listRooms includes defaults and created", "[room_manager][listRooms]") {
  auto before = rooms().listRooms();
  REQUIRE(before.size() >= 2);
  bool hasLobby = false;
  bool hasGeneral = false;
  for (const Room &r : before) {
    if (r.getName() == RoomManager::LOBBY.getName()) {
      hasLobby = true;
    }
    if (r.getName() == RoomManager::GENERAL.getName()) {
      hasGeneral = true;
    }
  }
  REQUIRE(hasLobby);
  REQUIRE(hasGeneral);

  const std::string name = unique("listed");
  REQUIRE(rooms().createRoom(Room(0, name)));
  auto after = rooms().listRooms();
  REQUIRE(after.size() == before.size() + 1);
  bool found = false;
  for (const Room &r : after) {
    if (r.getName() == name) {
      found = true;
      break;
    }
  }
  REQUIRE(found);

  ConnectionManager connections;
  REQUIRE(rooms().deleteRoom(name, connections));
  REQUIRE(rooms().listRooms().size() == before.size());
}

// 17. leaveAll is idempotent for Lobby-only session
TEST_CASE("RoomManager leaveAll is idempotent for Lobby-only session",
          "[room_manager][leaveAll][edge]") {
  TrackedSession tracked(nextFakeSocket());
  REQUIRE(tracked.get().getRoom().getName() == RoomManager::LOBBY.getName());
  rooms().leaveAll(tracked.get());
  rooms().leaveAll(tracked.get());
  REQUIRE(tracked.get().getRoom().getName() == RoomManager::LOBBY.getName());
}

// 18. broadcastAll respects skipSocket
TEST_CASE("RoomManager broadcastAll respects skipSocket",
          "[room_manager][broadcastAll][edge]") {
  REQUIRE(winsock().ok);
  ensureRandomRoom();
  NetFixture net;
  REQUIRE(net.listen());

  auto c1 = net.connectClient();
  auto c2 = net.connectClient();
  REQUIRE(c1.client != INVALID_SOCKET);
  REQUIRE(c2.client != INVALID_SOCKET);
  REQUIRE(c1.session);
  REQUIRE(c2.session);

  REQUIRE(rooms().joinRoom("General", *c1.session));
  REQUIRE(rooms().joinRoom("Random", *c2.session));
  REQUIRE(setRecvTimeout(c1.client, 500));
  REQUIRE(setRecvTimeout(c2.client, 500));

  Packet packet = makeMsg("skip-me");
  REQUIRE(rooms().broadcastAll(packet, net.connections, c1.session->getSocket()));

  auto skipped = socket_io::readPacket(c1.client);
  REQUIRE_FALSE(skipped.has_value());
  auto got = socket_io::readPacket(c2.client);
  REQUIRE(got);
  REQUIRE(got->message == "skip-me");

  rooms().leaveAll(*c1.session);
  rooms().leaveAll(*c2.session);
}
