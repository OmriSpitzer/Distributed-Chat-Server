/**
 * Client unit tests
 *
 * @brief Includes: start / stop / isAlive edges, home Exit disconnects,
 * login / register success round-trips, login failure, login then logout.
 * @date 13-09-2026
 */

#include "client/client.h"
#include "config/config.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/packet.h"
#include "utils/models/user.h"
#include "utils/socket_io.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

// Windows headers #define ERROR; clashes with RESPONSE_CODES::ERROR
#ifdef ERROR
#undef ERROR
#endif

/**
 * 1. default is not alive
 * 2. start fails when nothing is listening
 * 3. start then stop
 * 4. double start fails
 * 5. stop without start is a no-op
 * 6. home Exit disconnects
 * 7. login success round-trip
 * 8. login failure keeps connection
 * 9. register success round-trip
 * 10. login then logout
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

Winsock &winsock() {
  static Winsock instance;
  return instance;
}

struct ConfigGuard {
  std::string host = config::SERVER_HOST;
  std::uint16_t port = config::PORT;
  ~ConfigGuard() {
    config::SERVER_HOST = host;
    config::PORT = port;
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
    return true;
  }

  bool acceptOnce() {
    peer = socket_io::acceptFrom(listener);
    return peer != INVALID_SOCKET;
  }
};

struct IoRedirect {
  std::istringstream in;
  std::ostringstream out;
  std::streambuf *oldIn;
  std::streambuf *oldOut;

  explicit IoRedirect(const std::string &input)
      : in(input), oldIn(std::cin.rdbuf(in.rdbuf())), oldOut(std::cout.rdbuf(out.rdbuf())) {
    std::cin.clear();
  }

  ~IoRedirect() {
    std::cin.rdbuf(oldIn);
    std::cout.rdbuf(oldOut);
    std::cin.clear();
  }

  IoRedirect(const IoRedirect &) = delete;
  IoRedirect &operator=(const IoRedirect &) = delete;
};

Packet authSuccess(Packet::PacketType type, const User &user) {
  return Packet("server", user.getUsername(), type, "", user.serialize(),
                static_cast<int>(RESPONSE_CODES::SUCCESS));
}

Packet simpleReply(Packet::PacketType type, int code, const std::string &message) {
  return Packet("server", "client", type, "", message, code);
}

bool startClient(Client &client, TestPeer &server) {
  std::thread acceptor([&server] { (void)server.acceptOnce(); });
  const bool ok = client.start();
  acceptor.join();
  return ok && server.peer != INVALID_SOCKET && client.isAlive();
}

} // namespace

// 1. default is not alive
TEST_CASE("Client default is not alive", "[client][ctor][edge]") {
  REQUIRE(winsock().ok);
  const Client client;
  REQUIRE_FALSE(client.isAlive());
}

// 2. start fails when nothing is listening
TEST_CASE("Client start fails when nothing is listening", "[client][start][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;

  SOCKET listener = socket_io::listenTo(0);
  REQUIRE(listener != INVALID_SOCKET);
  sockaddr_in bound{};
  int boundLen = sizeof(bound);
  REQUIRE(getsockname(listener, reinterpret_cast<sockaddr *>(&bound), &boundLen) == 0);
  config::SERVER_HOST = "127.0.0.1";
  config::PORT = ntohs(bound.sin_port);
  socket_io::close(listener);

  Client client;
  REQUIRE_FALSE(client.start());
  REQUIRE_FALSE(client.isAlive());
}

// 3. start then stop
TEST_CASE("Client start then stop", "[client][start][stop]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Client client;
  REQUIRE(startClient(client, server));
  client.stop();
  REQUIRE_FALSE(client.isAlive());
}

// 4. double start fails
TEST_CASE("Client double start fails", "[client][start][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Client client;
  REQUIRE(startClient(client, server));
  REQUIRE_FALSE(client.start());
  REQUIRE(client.isAlive());
}

// 5. stop without start is a no-op
TEST_CASE("Client stop without start is a no-op", "[client][stop][edge]") {
  REQUIRE(winsock().ok);
  Client client;
  client.stop();
  REQUIRE_FALSE(client.isAlive());
}

// 6. home Exit disconnects
TEST_CASE("Client home Exit disconnects", "[client][dashboard]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  Client client;
  REQUIRE(startClient(client, server));

  {
    IoRedirect io("3\n");
    client.showDashboard();
  }

  REQUIRE_FALSE(client.isAlive());
}

// 7. login success round-trip
TEST_CASE("Client login success round-trip", "[client][login]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  const User user("alice", "alice@example.com", User::UserType::USER);

  std::thread serverThread([&] {
    REQUIRE(server.acceptOnce());
    const auto req = socket_io::readPacket(server.peer);
    REQUIRE(req.has_value());
    REQUIRE(req->type == Packet::PacketType::LOGIN);
    REQUIRE(req->sender == "alice");
    REQUIRE(socket_io::writePacket(server.peer, authSuccess(Packet::PacketType::LOGIN, user)));
  });

  Client client;
  REQUIRE(client.start());

  {
    IoRedirect io("1\nalice\nsecret\n");
    client.showDashboard();
  }

  serverThread.join();
  REQUIRE(client.isAlive());
}

// 8. login failure keeps connection
TEST_CASE("Client login failure keeps connection", "[client][login][edge]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  std::thread serverThread([&] {
    REQUIRE(server.acceptOnce());
    const auto req = socket_io::readPacket(server.peer);
    REQUIRE(req.has_value());
    REQUIRE(req->type == Packet::PacketType::LOGIN);
    const Packet reply = simpleReply(Packet::PacketType::LOGIN,
                                     static_cast<int>(RESPONSE_CODES::ERROR), "bad credentials");
    REQUIRE(socket_io::writePacket(server.peer, reply));
  });

  Client client;
  REQUIRE(client.start());

  {
    IoRedirect io("1\nalice\nwrong\n");
    client.showDashboard();
  }

  serverThread.join();
  REQUIRE(client.isAlive());

  // still on home menu — Exit should disconnect
  {
    IoRedirect io("3\n");
    client.showDashboard();
  }
  REQUIRE_FALSE(client.isAlive());
}

// 9. register success round-trip
TEST_CASE("Client register success round-trip", "[client][register]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  const User user("bob", "bob@example.com", User::UserType::USER);

  std::thread serverThread([&] {
    REQUIRE(server.acceptOnce());
    const auto req = socket_io::readPacket(server.peer);
    REQUIRE(req.has_value());
    REQUIRE(req->type == Packet::PacketType::REGISTER);
    REQUIRE(req->sender == "bob");
    REQUIRE(req->room == "bob@example.com");
    REQUIRE(socket_io::writePacket(server.peer, authSuccess(Packet::PacketType::REGISTER, user)));
  });

  Client client;
  REQUIRE(client.start());

  {
    IoRedirect io("2\nbob\npass\nbob@example.com\n");
    client.showDashboard();
  }

  serverThread.join();
  REQUIRE(client.isAlive());
}

// 10. login then logout
TEST_CASE("Client login then logout", "[client][logout]") {
  REQUIRE(winsock().ok);
  ConfigGuard guard;
  TestPeer server;
  REQUIRE(server.listen());

  const User user("alice", "alice@example.com", User::UserType::USER);

  std::thread serverThread([&] {
    REQUIRE(server.acceptOnce());

    const auto login = socket_io::readPacket(server.peer);
    REQUIRE(login.has_value());
    REQUIRE(login->type == Packet::PacketType::LOGIN);
    REQUIRE(socket_io::writePacket(server.peer, authSuccess(Packet::PacketType::LOGIN, user)));

    const auto logout = socket_io::readPacket(server.peer);
    REQUIRE(logout.has_value());
    REQUIRE(logout->type == Packet::PacketType::LOGOUT);
    REQUIRE(logout->sender == "alice");
    const Packet logoutReply =
        simpleReply(Packet::PacketType::LOGOUT, static_cast<int>(RESPONSE_CODES::SUCCESS),
                    "logged out");
    REQUIRE(socket_io::writePacket(server.peer, logoutReply));
  });

  Client client;
  REQUIRE(client.start());

  {
    IoRedirect io("1\nalice\nsecret\n");
    client.showDashboard();
  }
  {
    // dashboard: 7 = Logout
    IoRedirect io("7\n");
    client.showDashboard();
  }

  serverThread.join();
  REQUIRE(client.isAlive());
}
