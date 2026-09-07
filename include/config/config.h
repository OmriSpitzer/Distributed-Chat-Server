/**
 * Configuration file for the server
 *
 * @date 07-09-2026
 */
#pragma once
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace config {
// server host
inline std::string SERVER_HOST = "127.0.0.1";

// TCP port the server listens on
inline std::uint16_t PORT = 5555;

// TCP port for gossip between servers
inline std::uint16_t PEER_PORT = 5557;

// this process's node id
inline std::string NODE_ID = "node1";

// other nodes' gossip addresses (host:port)
inline std::vector<std::string> PEERS;

// number of worker threads in the thread pool
inline constexpr std::size_t THREAD_COUNT = 4;

// path to the database file
inline std::string DB_PATH = "data/node-1.db";

// heartbeat interval in milliseconds
inline constexpr int HEARTBEAT_INTERVAL = 5000;

// heartbeat timeout in milliseconds
inline constexpr int HEARTBEAT_TIMEOUT = 10000;

// print the usage of the program
inline void printUsage(const char *program) {
  std::cerr << "Usage: " << program << " [options]\n"
            << "  --node-id ID          node identity (default: node1)\n"
            << "  --host HOST           client connect host (default: 127.0.0.1)\n"
            << "  --port N              client TCP port (default: 5555)\n"
            << "  --peer-port N         gossip listen port (default: 5557)\n"
            << "  --peers H:P,H:P       other nodes' gossip addresses\n"
            << "  --db PATH             database path (default: data/node-1.db)\n"
            << "  --help                show this help\n";
}

// parse a port from a string
inline bool parsePort(std::string_view text, std::uint16_t &out) {
  try {
    unsigned long value = std::stoul(std::string(text));
    if (value == 0 || value > 65535) {
      return false;
    }
    out = static_cast<std::uint16_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

// parse a list of peers from a string
inline void parsePeers(std::string_view text) {
  PEERS.clear();
  std::stringstream stream{std::string(text)};
  std::string item;
  while (std::getline(stream, item, ',')) {
    if (!item.empty()) {
      PEERS.push_back(item);
    }
  }
}

// parse the command line arguments into config values. returns false on --help or error.
inline bool parseArgs(int argc, char *argv[]) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];

    auto next = [&](const char *name) -> const char * {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << name << "\n";
        printUsage(argv[0]);
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "--help" || arg == "-h") {
      printUsage(argv[0]);
      return false;
    }

    if (arg == "--node-id") {
      const char *value = next("--node-id");
      if (!value) {
        return false;
      }
      NODE_ID = value;
      continue;
    }

    if (arg == "--host") {
      const char *value = next("--host");
      if (!value) {
        return false;
      }
      SERVER_HOST = value;
      continue;
    }

    if (arg == "--port") {
      const char *value = next("--port");
      if (!value) {
        return false;
      }
      if (!parsePort(value, PORT)) {
        std::cerr << "Invalid --port\n";
        return false;
      }
      continue;
    }

    if (arg == "--peer-port") {
      const char *value = next("--peer-port");
      if (!value) {
        return false;
      }
      if (!parsePort(value, PEER_PORT)) {
        std::cerr << "Invalid --peer-port\n";
        return false;
      }
      continue;
    }

    if (arg == "--peers") {
      const char *value = next("--peers");
      if (!value) {
        return false;
      }
      parsePeers(value);
      continue;
    }

    if (arg == "--db") {
      const char *value = next("--db");
      if (!value) {
        return false;
      }
      DB_PATH = value;
      continue;
    }

    std::cerr << "Unknown argument: " << arg << "\n";
    printUsage(argv[0]);
    return false;
  }

  return true;
}

} // namespace config