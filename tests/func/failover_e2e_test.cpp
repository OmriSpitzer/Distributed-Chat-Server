/**
 * E2E: kill node A while a logged-in client is in a room
 *
 * @brief Spawns two chat_server processes. The client caches SERVER_DIRECTORY,
 * fails over to node B after A is killed, keeps the same user and room, and
 * receives MESSAGE again.
 * @date 29-09-2026
 */

#include "client/client.h"
#include "config/config.h"
#include "utils/socket_io.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif

/**
 * 1. kill node A: cached directory, same user and room, MESSAGE on node B
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

template <typename Predicate> bool waitUntil(std::chrono::milliseconds timeout, Predicate pred) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  do {
    if (pred()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  } while (std::chrono::steady_clock::now() < deadline);
  return pred();
}

bool portAccepts(std::uint16_t port) {
  const SOCKET socket = socket_io::connectTo("127.0.0.1", port);
  if (socket == INVALID_SOCKET) {
    return false;
  }
  socket_io::close(socket);
  return true;
}

std::wstring quoteArg(const std::wstring &value) {
  std::wstring out = L"\"";
  for (wchar_t ch : value) {
    if (ch == L'"') {
      out += L"\\\"";
    } else {
      out += ch;
    }
  }
  out += L'"';
  return out;
}

// one chat_server.exe; killed in the destructor so a failed REQUIRE does not leak it
struct ServerProcess {
  PROCESS_INFORMATION process{};
  HANDLE log{INVALID_HANDLE_VALUE};
  std::filesystem::path logPath;
  bool running{false};

  ServerProcess() = default;
  ServerProcess(const ServerProcess &) = delete;
  ServerProcess &operator=(const ServerProcess &) = delete;

  ~ServerProcess() { stop(); }

  bool start(const std::filesystem::path &exe, const std::wstring &command,
             const std::filesystem::path &logFile) {
    logPath = logFile;
    SECURITY_ATTRIBUTES inherit{};
    inherit.nLength = sizeof(inherit);
    inherit.bInheritHandle = TRUE;

    log = CreateFileW(logFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &inherit, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (log == INVALID_HANDLE_VALUE) {
      return false;
    }

    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = nul;
    startup.hStdOutput = log;
    startup.hStdError = log;

    std::wstring mutableCommand = command;
    const std::wstring workDir = exe.parent_path().wstring();
    const BOOL created =
        CreateProcessW(exe.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP, nullptr, workDir.c_str(),
                       &startup, &process);
    if (nul != INVALID_HANDLE_VALUE) {
      CloseHandle(nul);
    }
    if (!created) {
      return false;
    }
    running = true;
    return true;
  }

  void stop() {
    if (running) {
      TerminateProcess(process.hProcess, 1);
      WaitForSingleObject(process.hProcess, 5000);
      CloseHandle(process.hProcess);
      CloseHandle(process.hThread);
      process = {};
      running = false;
    }
    if (log != INVALID_HANDLE_VALUE) {
      CloseHandle(log);
      log = INVALID_HANDLE_VALUE;
    }
  }

  std::string readLog() const {
    std::ifstream in(logPath);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
  }
};

struct ConfigGuard {
  std::string host = config::SERVER_HOST;
  std::uint16_t port = config::PORT;
  std::vector<ServerPoint> neighbors = config::NEIGHBOR_SERVERS;
  bool testMode = config::TEST_MODE;

  ~ConfigGuard() {
    config::SERVER_HOST = host;
    config::PORT = port;
    config::NEIGHBOR_SERVERS = neighbors;
    config::TEST_MODE = testMode;
  }
};

bool directoryHasPort(const Client &client, std::uint16_t port) {
  for (const ClientEndpoint &endpoint : client.getState().getServerEndpoints()) {
    if (endpoint.port == port && endpoint.isLiveClientEndpoint()) {
      return true;
    }
  }
  return false;
}

std::string directoryText(const Client &client) {
  std::ostringstream out;
  for (const ClientEndpoint &endpoint : client.getState().getServerEndpoints()) {
    out << endpoint.nodeId << "=" << endpoint.host << ":" << endpoint.port << " ";
  }
  return out.str();
}

bool waitForPush(Client &client, const std::string &author, const std::string &text,
                 std::chrono::milliseconds timeout = std::chrono::seconds(8)) {
  return waitUntil(timeout, [&] {
    for (const ChatLine &line : client.takePendingChatMessages()) {
      if (line.author == author && line.text == text) {
        return true;
      }
    }
    return false;
  });
}

std::filesystem::path chatServerExe() {
  wchar_t buffer[32768];
  const DWORD length = GetModuleFileNameW(nullptr, buffer, 32768);
  REQUIRE(length > 0);
  return std::filesystem::path(buffer).parent_path() / "chat_server.exe";
}

} // namespace

// E2E kill node A: client lands on B still as the same user in the same room
TEST_CASE("E2E kill node A logged-in client fails over to B",
          "[func][e2e][failover][slow]") {
  REQUIRE(winsock().ok);

  const auto exe = chatServerExe();
  INFO("chat_server: " << exe.string());
  REQUIRE(std::filesystem::exists(exe));

  const std::uint16_t base =
      static_cast<std::uint16_t>(40000 + (GetCurrentProcessId() % 3000) * 8);
  const std::uint16_t portA = base;
  const std::uint16_t peerA = static_cast<std::uint16_t>(base + 1);
  const std::uint16_t wsA = static_cast<std::uint16_t>(base + 2);
  const std::uint16_t portB = static_cast<std::uint16_t>(base + 3);
  const std::uint16_t peerB = static_cast<std::uint16_t>(base + 4);
  const std::uint16_t wsB = static_cast<std::uint16_t>(base + 5);

  const auto dir = exe.parent_path() / "failover-e2e" / std::to_string(GetCurrentProcessId());
  std::filesystem::create_directories(dir);
  const auto dbA = dir / "node-a.db";
  const auto dbB = dir / "node-b.db";
  std::filesystem::remove(dbA);
  std::filesystem::remove(dbB);

  auto commandFor = [&](const wchar_t *nodeId, std::uint16_t port, std::uint16_t peer,
                        std::uint16_t ws, std::uint16_t otherPeer, const std::filesystem::path &db) {
    std::wstring command = quoteArg(exe.wstring());
    command += L" --test --node-id ";
    command += nodeId;
    command += L" --host 127.0.0.1 --port " + std::to_wstring(port);
    command += L" --peer-port " + std::to_wstring(peer);
    command += L" --ws-port " + std::to_wstring(ws);
    command += L" --peers 127.0.0.1:" + std::to_wstring(otherPeer);
    command += L" --db " + quoteArg(db.wstring());
    return command;
  };

  ServerProcess nodeA;
  ServerProcess nodeB;
  const auto logA = dir / "node-a.log";
  const auto logB = dir / "node-b.log";
  REQUIRE(nodeA.start(exe, commandFor(L"node-a", portA, peerA, wsA, peerB, dbA), logA));
  REQUIRE(nodeB.start(exe, commandFor(L"node-b", portB, peerB, wsB, peerA, dbB), logB));

  INFO("node A log:\n" << nodeA.readLog());
  INFO("node B log:\n" << nodeB.readLog());
  REQUIRE(waitUntil(std::chrono::seconds(8), [&] { return portAccepts(portA) && portAccepts(portB); }));

  ConfigGuard guard;
  config::TEST_MODE = true;
  config::SERVER_HOST = "127.0.0.1";

  // bob stays on B; alice's seed is A then B so the hop after A dies is the cached peer
  config::PORT = portB;
  config::NEIGHBOR_SERVERS.clear();
  Client bob;
  REQUIRE(bob.start());
  REQUIRE(bob.isAlive());

  config::PORT = portA;
  config::NEIGHBOR_SERVERS = {ServerPoint{"127.0.0.1", portB}};
  Client alice;
  REQUIRE(alice.start());
  REQUIRE(alice.isAlive());

  INFO("alice directory: " << directoryText(alice));
  REQUIRE(waitUntil(std::chrono::seconds(15), [&] { return directoryHasPort(alice, portB); }));

  REQUIRE(alice.login("admin", "admin").empty());
  REQUIRE(bob.login("user", "user").empty());
  REQUIRE(alice.joinRoom("General").empty());
  REQUIRE(bob.joinRoom("General").empty());
  REQUIRE(alice.getState().currentRoom->getName() == "General");
  REQUIRE(bob.getState().currentRoom->getName() == "General");

  alice.clearPendingChatMessages();
  bob.clearPendingChatMessages();
  REQUIRE(alice.sendMessage("before failover").empty());
  INFO("node A log:\n" << nodeA.readLog());
  INFO("node B log:\n" << nodeB.readLog());
  REQUIRE(waitForPush(bob, "admin", "before failover"));

  nodeA.stop();
  REQUIRE(waitUntil(std::chrono::seconds(15), [&] {
    return alice.isAlive() && !alice.isReconnecting() && alice.reconnectCount() >= 1 &&
           alice.getState().isLoggedIn() && alice.getState().user &&
           alice.getState().user->getUsername() == "admin" && alice.getState().currentRoom &&
           alice.getState().currentRoom->getName() == "General";
  }));

  // same in-memory user and room; login() is not called again
  REQUIRE(alice.getState().isLoggedIn());
  REQUIRE(alice.getState().user->getUsername() == "admin");
  REQUIRE(alice.getState().currentRoom->getName() == "General");
  REQUIRE(bob.isAlive());
  REQUIRE(bob.getState().user->getUsername() == "user");

  alice.clearPendingChatMessages();
  REQUIRE(bob.sendMessage("after failover").empty());
  INFO("node B log:\n" << nodeB.readLog());
  REQUIRE(waitForPush(alice, "user", "after failover"));

  alice.stop();
  bob.stop();
}
