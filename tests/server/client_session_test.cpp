/**
 * ClientSession unit tests
 *
 * @brief Includes: constructor defaults, edge sockets, setUser / getUser, setRoom / getRoom,
 * authentication flag, markClosed is one-shot, concurrent markClosed, sendMutex can be locked,
 * heartbeat touch / isAlive, concurrent getters and setters, typical login/logout flow.
 * @date 11-09-2026
 */

#include "config/config.h"
#include "server/client_session.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

/**
 * 1. constructor defaults
 * 2. edge sockets
 * 3. setUser / getUser
 * 4. setRoom / getRoom
 * 5. authentication flag
 * 6. markClosed is one-shot
 * 7. concurrent markClosed
 * 8. sendMutex can be locked
 * 9. heartbeat touch / isAlive
 * 10. concurrent getters and setters
 * 11. typical login/logout flow
 */

// global constants
static const User kAnon = User::anonymousUser();
static const User kAlice("alice", "alice@example.com", User::UserType::USER);
static const User kBob("bob", "bob@example.com", User::UserType::ADMIN);
static const Room kLobby("Lobby", Room::RoomType::LOBBY);
static const Room kGeneral("General", Room::RoomType::OTHER);
static const Room kSecure("secure", Room::RoomType::SECURITY, Room::Privacy::PRIVATE);

// 1. constructor defaults
TEST_CASE("ClientSession constructor defaults", "[client_session][ctor]") {
  ClientSession session(42, kAnon, kLobby);

  // check defaults
  REQUIRE(session.getSocket() == 42);
  REQUIRE(session.getUser() == kAnon);
  REQUIRE(session.getRoom().getName() == "Lobby");
  REQUIRE(session.getRoom().getType() == Room::RoomType::LOBBY);
  REQUIRE_FALSE(session.isAuthenticated());
  REQUIRE_FALSE(session.isClosed());
  REQUIRE(session.isAlive());
}

// 2. edge sockets
TEST_CASE("ClientSession constructor edge sockets", "[client_session][ctor][edge]") {
  // zero socket
  SECTION("zero socket") {
    ClientSession session(0, kAlice, kGeneral);
    REQUIRE(session.getSocket() == 0);
  }

  // negative socket
  SECTION("negative socket") {
    ClientSession session(-1, kAlice, kGeneral);
    REQUIRE(session.getSocket() == -1);
  }

  // large socket fd
  SECTION("large socket fd") {
    ClientSession session(65535, kBob, kSecure);
    REQUIRE(session.getSocket() == 65535);
    REQUIRE(session.getUser() == kBob);
    REQUIRE(session.getRoom().getName() == "secure");
  }
}

// 3. setUser / getUser
TEST_CASE("ClientSession setUser / getUser", "[client_session][user]") {
  ClientSession session(1, kAnon, kLobby);

  // replace anonymous with alice
  SECTION("replace anonymous with alice") {
    session.setUser(kAlice);
    REQUIRE(session.getUser() == kAlice);
    REQUIRE(session.getUser().getUsername() == "alice");
  }

  // replace again with bob
  SECTION("replace again with bob") {
    session.setUser(kAlice);
    session.setUser(kBob);
    REQUIRE(session.getUser() == kBob);
  }

  // set back to anonymous
  SECTION("set back to anonymous") {
    session.setUser(kAlice);
    session.setUser(User::anonymousUser());
    REQUIRE(session.getUser().getUserType() == User::UserType::GUEST);
    REQUIRE(session.getUser().getUsername().rfind("anon", 0) == 0);
  }

  // getters return copies (mutation of returned value does not affect session)
  SECTION("getters return copies (mutation of returned value does not affect session)") {
    session.setUser(kAlice);
    User copy = session.getUser();
    copy.setUsername("mutated");
    REQUIRE(session.getUser().getUsername() == "alice");
  }
}

// 4. setRoom / getRoom
TEST_CASE("ClientSession setRoom / getRoom", "[client_session][room]") {
  ClientSession session(2, kAlice, kLobby);

  // join general
  SECTION("join general") {
    session.setRoom(kGeneral);
    REQUIRE(session.getRoom().getName() == "General");
    REQUIRE(session.getRoom().getType() == Room::RoomType::OTHER);
  }

  // join private room
  SECTION("join private room") {
    session.setRoom(kSecure);
    REQUIRE(session.getRoom().getName() == "secure");
    REQUIRE(session.getRoom().getPrivacy() == Room::Privacy::PRIVATE);
  }

  // return to lobby
  SECTION("return to lobby") {
    session.setRoom(kSecure);
    session.setRoom(kLobby);
    REQUIRE(session.getRoom().getName() == "Lobby");
  }

  // getRoom returns a copy
  SECTION("getRoom returns a copy") {
    session.setRoom(kGeneral);
    Room copy = session.getRoom();
    REQUIRE(copy.getId() == session.getRoom().getId());
    REQUIRE(copy.getName() == "General");
  }
}

// 5. authentication flag
TEST_CASE("ClientSession authentication flag", "[client_session][auth]") {
  ClientSession session(3, kAnon, kLobby);

  // check initial state
  REQUIRE_FALSE(session.isAuthenticated());

  // authenticate
  SECTION("authenticate") {
    session.setUser(kAlice);
    session.setAuthenticated(true);
    REQUIRE(session.isAuthenticated());
    REQUIRE(session.getUser() == kAlice);
  }

  // logout clears auth
  SECTION("logout clears auth") {
    session.setUser(kAlice);
    session.setAuthenticated(true);
    session.setAuthenticated(false);
    session.setUser(kAnon);
    REQUIRE_FALSE(session.isAuthenticated());
  }

  // auth true then false then true
  SECTION("auth true then false then true") {
    session.setAuthenticated(true);
    session.setAuthenticated(false);
    session.setAuthenticated(true);
    REQUIRE(session.isAuthenticated());
  }
}

// 6. markClosed is one-shot
TEST_CASE("ClientSession markClosed is one-shot", "[client_session][close]") {
  ClientSession session(4, kAlice, kLobby);

  // check initial state
  REQUIRE_FALSE(session.isClosed());
  REQUIRE(session.markClosed());
  REQUIRE(session.isClosed());

  // second markClosed returns false
  SECTION("second markClosed returns false") {
    REQUIRE_FALSE(session.markClosed());
    REQUIRE(session.isClosed());
  }

  // state remains readable after close
  SECTION("state remains readable after close") {
    REQUIRE(session.getSocket() == 4);
    REQUIRE(session.getUser() == kAlice);
    REQUIRE_FALSE(session.isAuthenticated());
  }
}

// 7. concurrent markClosed
TEST_CASE("ClientSession markClosed concurrent", "[client_session][close][thread]") {
  ClientSession session(5, kAlice, kLobby);

  // check initial state
  std::atomic<int> wins{0};
  auto closer = [&]() {
    if (session.markClosed()) {
      wins.fetch_add(1);
    }
  };

  // create two threads to close the session
  std::thread t1(closer);
  std::thread t2(closer);
  t1.join();
  t2.join();

  // check session is closed and only one thread won
  REQUIRE(session.isClosed());
  REQUIRE(wins.load() == 1);
}

// 8. sendMutex can be locked
TEST_CASE("ClientSession sendMutex can be locked", "[client_session][mutex]") {
  ClientSession session(6, kAlice, kLobby);

  // check session is not closed
  {
    std::lock_guard<std::mutex> lock(session.sendMutex());
    REQUIRE_FALSE(session.isClosed());
  }

  // lock from two sequential critical sections
  SECTION("lock from two sequential critical sections") {
    {
      std::lock_guard<std::mutex> lock(session.sendMutex());
    }
    {
      std::lock_guard<std::mutex> lock(session.sendMutex());
    }
    // check success
    SUCCEED();
  }
}

// 9. heartbeat touch / isAlive
TEST_CASE("ClientSession heartbeat touch / isAlive", "[client_session][heartbeat]") {
  ClientSession session(7, kAlice, kLobby);

  // check session is alive
  REQUIRE(session.isAlive());

  // touch keeps session alive
  SECTION("touch keeps session alive") {
    session.touch();
    REQUIRE(session.isAlive());
  }

  // repeated touch
  SECTION("repeated touch") {
    for (int i = 0; i < 5; ++i) {
      session.touch();
      REQUIRE(session.isAlive());
    }
  }

  // expires after HEARTBEAT_TIMEOUT
  SECTION("expires after HEARTBEAT_TIMEOUT") {
    // Intentionally slow: waits past config::HEARTBEAT_TIMEOUT (10s)
    std::this_thread::sleep_for(std::chrono::milliseconds(config::HEARTBEAT_TIMEOUT + 500));
    REQUIRE_FALSE(session.isAlive());

    session.touch();
    REQUIRE(session.isAlive());
  }
}

// 10. concurrent getters and setters
TEST_CASE("ClientSession concurrent getters and setters", "[client_session][thread]") {
  ClientSession session(8, kAnon, kLobby);

  // create two threads to set and get session state
  std::atomic<bool> run{true};
  std::thread writer([&]() {
    while (run.load()) {
      session.setUser(kAlice);
      session.setAuthenticated(true);
      session.setRoom(kGeneral);
      session.touch();
      session.setUser(kBob);
      session.setRoom(kLobby);
      session.setAuthenticated(false);
    }
  });

  // create a thread to read session state
  std::thread reader([&]() {
    for (int i = 0; i < 2000; ++i) {
      (void)session.getUser();
      (void)session.getRoom();
      (void)session.isAuthenticated();
      (void)session.isAlive();
      (void)session.isClosed();
    }
  });

  // join threads and stop writer
  reader.join();
  run.store(false);
  writer.join();

  // check session is not closed
  REQUIRE_FALSE(session.isClosed());
  SUCCEED();
}

// 11. typical login/logout flow
TEST_CASE("ClientSession typical login/logout flow", "[client_session][flow]") {
  ClientSession session(9, User::anonymousUser(), kLobby);

  // check initial state
  REQUIRE_FALSE(session.isAuthenticated());
  REQUIRE(session.getRoom().getName() == "Lobby");

  // login as alice
  session.setUser(kAlice);
  session.setAuthenticated(true);
  session.setRoom(kGeneral);
  session.touch();

  // check session state
  REQUIRE(session.isAuthenticated());
  REQUIRE(session.getUser() == kAlice);
  REQUIRE(session.getRoom().getName() == "General");
  REQUIRE(session.isAlive());

  // logout
  session.setRoom(kLobby);
  session.setAuthenticated(false);
  session.setUser(User::anonymousUser());

  // check session state
  REQUIRE_FALSE(session.isAuthenticated());
  REQUIRE(session.getUser().getUserType() == User::UserType::GUEST);
  REQUIRE(session.getRoom().getName() == "Lobby");
}