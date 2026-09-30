/**
 * CLI argument parsing tests
 *
 * @brief Includes: parsePort bounds, parsePeers, parseNeighborServers / --servers,
 * parseArgs flags, unknown flags, missing values, and --help.
 * @date 30-09-2026
 */

#include "config/config.h"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

/**
 * 1. parsePort accepts in-range ports
 * 2. parsePort rejects invalid text
 * 3. parsePeers splits commas and skips empty items
 * 4. parseNeighborServers stores endpoints
 * 5. parseNeighborServers skips the primary endpoint and duplicates
 * 6. parseNeighborServers rejects a bad entry
 * 7. parseArgs applies flags including --servers
 * 8. parseArgs --host and --port drop a matching neighbor
 * 9. parseArgs rejects help, unknown flags, missing values, and bad ports
 */

namespace {

struct ConfigGuard {
  std::string host = config::SERVER_HOST;
  std::uint16_t port = config::PORT;
  std::uint16_t wsPort = config::WS_PORT;
  std::uint16_t peerPort = config::PEER_PORT;
  std::string nodeId = config::NODE_ID;
  std::vector<std::string> peers = config::PEERS;
  std::vector<ServerPoint> neighbors = config::NEIGHBOR_SERVERS;
  std::string dbPath = config::DB_PATH;
  bool testMode = config::TEST_MODE;

  ~ConfigGuard() {
    config::SERVER_HOST = host;
    config::PORT = port;
    config::WS_PORT = wsPort;
    config::PEER_PORT = peerPort;
    config::NODE_ID = nodeId;
    config::PEERS = peers;
    config::NEIGHBOR_SERVERS = neighbors;
    config::DB_PATH = dbPath;
    config::TEST_MODE = testMode;
  }
};

struct CerrCapture {
  std::stringstream sink;
  std::streambuf *previous;

  CerrCapture() : previous(std::cerr.rdbuf(sink.rdbuf())) {}
  ~CerrCapture() { std::cerr.rdbuf(previous); }

  std::string str() const { return sink.str(); }
};

bool parse(std::initializer_list<std::string> args) {
  std::vector<std::string> storage;
  storage.reserve(args.size() + 1);
  storage.emplace_back("chat_server");
  for (const std::string &arg : args) {
    storage.push_back(arg);
  }

  std::vector<char *> argv;
  argv.reserve(storage.size());
  for (std::string &item : storage) {
    argv.push_back(item.data());
  }
  return config::parseArgs(static_cast<int>(argv.size()), argv.data());
}

} // namespace

// 1. parsePort accepts in-range ports
TEST_CASE("parsePort accepts in-range ports", "[config][port][flow]") {
  std::uint16_t port = 0;
  REQUIRE(config::parsePort("1", port));
  REQUIRE(port == 1);
  REQUIRE(config::parsePort("5555", port));
  REQUIRE(port == 5555);
  REQUIRE(config::parsePort("65535", port));
  REQUIRE(port == 65535);
}

// 2. parsePort rejects invalid text and leaves the output unchanged
TEST_CASE("parsePort rejects invalid text", "[config][port][edge]") {
  std::uint16_t port = 5555;

  SECTION("zero") { REQUIRE_FALSE(config::parsePort("0", port)); }
  SECTION("above 65535") { REQUIRE_FALSE(config::parsePort("65536", port)); }
  SECTION("empty") { REQUIRE_FALSE(config::parsePort("", port)); }
  SECTION("letters") { REQUIRE_FALSE(config::parsePort("abc", port)); }
  SECTION("negative") { REQUIRE_FALSE(config::parsePort("-1", port)); }

  REQUIRE(port == 5555);
}

// 3. parsePeers splits commas and skips empty items
TEST_CASE("parsePeers splits commas and skips empty items", "[config][peers]") {
  ConfigGuard guard;

  config::parsePeers("127.0.0.1:5557,,10.0.0.2:5558,");
  REQUIRE(config::PEERS.size() == 2);
  REQUIRE(config::PEERS[0] == "127.0.0.1:5557");
  REQUIRE(config::PEERS[1] == "10.0.0.2:5558");

  config::parsePeers("");
  REQUIRE(config::PEERS.empty());

  config::parsePeers("  ");
  REQUIRE(config::PEERS.size() == 1);
  REQUIRE(config::PEERS[0] == "  ");
}

// 4. parseNeighborServers stores host and port
TEST_CASE("parseNeighborServers stores endpoints", "[config][servers][flow]") {
  ConfigGuard guard;
  config::SERVER_HOST = "127.0.0.1";
  config::PORT = 5555;

  REQUIRE(config::parseNeighborServers("10.0.0.2:5556,10.0.0.3:5557"));
  REQUIRE(config::NEIGHBOR_SERVERS.size() == 2);
  REQUIRE(config::NEIGHBOR_SERVERS[0].host == "10.0.0.2");
  REQUIRE(config::NEIGHBOR_SERVERS[0].port == 5556);
  REQUIRE(config::NEIGHBOR_SERVERS[1].host == "10.0.0.3");
  REQUIRE(config::NEIGHBOR_SERVERS[1].port == 5557);

  REQUIRE(config::parseNeighborServers(",10.0.0.4:5558,"));
  REQUIRE(config::NEIGHBOR_SERVERS.size() == 1);
  REQUIRE(config::NEIGHBOR_SERVERS[0].host == "10.0.0.4");
  REQUIRE(config::NEIGHBOR_SERVERS[0].port == 5558);
}

// 5. primary --host/--port is not a neighbor; duplicates collapse
TEST_CASE("parseNeighborServers skips the primary endpoint and duplicates",
          "[config][servers][edge]") {
  ConfigGuard guard;
  config::SERVER_HOST = "127.0.0.1";
  config::PORT = 5555;

  REQUIRE(config::parseNeighborServers("127.0.0.1:5555,10.0.0.2:5556,10.0.0.2:5556"));
  REQUIRE(config::NEIGHBOR_SERVERS.size() == 1);
  REQUIRE(config::NEIGHBOR_SERVERS[0].host == "10.0.0.2");
  REQUIRE(config::NEIGHBOR_SERVERS[0].port == 5556);
}

// 6. a bad entry fails and clears the list
TEST_CASE("parseNeighborServers rejects a bad entry", "[config][servers][edge]") {
  ConfigGuard guard;
  config::SERVER_HOST = "127.0.0.1";
  config::PORT = 5555;
  REQUIRE(config::parseNeighborServers("10.0.0.2:5556"));
  REQUIRE(config::NEIGHBOR_SERVERS.size() == 1);

  SECTION("missing colon") { REQUIRE_FALSE(config::parseNeighborServers("10.0.0.2:5556,nocolon")); }
  SECTION("empty host") { REQUIRE_FALSE(config::parseNeighborServers(":5556")); }
  SECTION("empty port") { REQUIRE_FALSE(config::parseNeighborServers("10.0.0.2:")); }
  SECTION("port zero") { REQUIRE_FALSE(config::parseNeighborServers("10.0.0.2:0")); }
  SECTION("port above 65535") { REQUIRE_FALSE(config::parseNeighborServers("10.0.0.2:65536")); }

  REQUIRE(config::NEIGHBOR_SERVERS.empty());
}

// 7. one argv sets every flag, and --servers skips the primary endpoint
TEST_CASE("parseArgs applies flags including servers", "[config][args][flow]") {
  ConfigGuard guard;

  REQUIRE(parse({"--node-id", "node9", "--host", "10.1.1.1", "--port", "6000", "--ws-port", "9090",
                 "--peer-port", "6001", "--peers", "10.1.1.2:6001,10.1.1.3:6001", "--servers",
                 "10.1.1.4:6000,10.1.1.1:6000", "--db", "data/custom.db", "--test"}));

  REQUIRE(config::NODE_ID == "node9");
  REQUIRE(config::SERVER_HOST == "10.1.1.1");
  REQUIRE(config::PORT == 6000);
  REQUIRE(config::WS_PORT == 9090);
  REQUIRE(config::PEER_PORT == 6001);
  REQUIRE(config::PEERS.size() == 2);
  REQUIRE(config::PEERS[0] == "10.1.1.2:6001");
  REQUIRE(config::PEERS[1] == "10.1.1.3:6001");
  REQUIRE(config::NEIGHBOR_SERVERS.size() == 1);
  REQUIRE(config::NEIGHBOR_SERVERS[0].host == "10.1.1.4");
  REQUIRE(config::NEIGHBOR_SERVERS[0].port == 6000);
  REQUIRE(config::DB_PATH == "data/custom.db");
  REQUIRE(config::TEST_MODE);
}

// 8. later --port / --host drop a neighbor that now matches the primary
TEST_CASE("parseArgs host and port drop a matching neighbor", "[config][servers]") {
  ConfigGuard guard;
  config::SERVER_HOST = "127.0.0.1";
  config::PORT = 5555;

  REQUIRE(parse({"--servers", "10.0.0.2:5556,127.0.0.1:5556"}));
  REQUIRE(config::NEIGHBOR_SERVERS.size() == 2);

  REQUIRE(parse({"--port", "5556"}));
  REQUIRE(config::PORT == 5556);
  REQUIRE(config::NEIGHBOR_SERVERS.size() == 1);
  REQUIRE(config::NEIGHBOR_SERVERS[0].host == "10.0.0.2");
  REQUIRE(config::NEIGHBOR_SERVERS[0].port == 5556);

  REQUIRE(parse({"--host", "10.0.0.2"}));
  REQUIRE(config::SERVER_HOST == "10.0.0.2");
  REQUIRE(config::NEIGHBOR_SERVERS.empty());
}

// 9. help, unknown flags, missing values, and invalid ports
TEST_CASE("parseArgs rejects help unknown flags missing values and bad ports", "[config][args][edge]") {
  ConfigGuard guard;
  config::PORT = 5555;
  config::SERVER_HOST = "127.0.0.1";
  REQUIRE(config::parseNeighborServers("10.0.0.2:5556"));

  SECTION("help") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--help"}));
    REQUIRE(capture.str().find("Usage:") != std::string::npos);
  }
  SECTION("short help") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"-h"}));
    REQUIRE(capture.str().find("Usage:") != std::string::npos);
  }
  SECTION("unknown flag") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--nope"}));
    REQUIRE(capture.str().find("Unknown argument:") != std::string::npos);
  }
  SECTION("missing port") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--port"}));
    REQUIRE(capture.str().find("Missing value for --port") != std::string::npos);
    REQUIRE(config::PORT == 5555);
  }
  SECTION("missing servers") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--servers"}));
    REQUIRE(capture.str().find("Missing value for --servers") != std::string::npos);
  }
  SECTION("invalid port leaves the previous value") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--port", "0"}));
    REQUIRE(capture.str().find("Invalid --port") != std::string::npos);
    REQUIRE(config::PORT == 5555);
  }
  SECTION("invalid ws port") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--ws-port", "abc"}));
    REQUIRE(capture.str().find("Invalid --ws-port") != std::string::npos);
  }
  SECTION("invalid peer port") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--peer-port", "65536"}));
    REQUIRE(capture.str().find("Invalid --peer-port") != std::string::npos);
  }
  SECTION("invalid servers clears the list") {
    CerrCapture capture;
    REQUIRE_FALSE(parse({"--servers", "nocolon"}));
    REQUIRE(capture.str().find("Invalid --servers") != std::string::npos);
    REQUIRE(config::NEIGHBOR_SERVERS.empty());
  }
}
