/**
 * Server class implementation file
 *
 * @brief Wires together the server components and owns the listen lifecycle
 *
 * Server class with fields: threadPool, connectionManager, gossipManager, heartbeat, acceptThread,
 * running
 * Used for managing the server lifecycle
 * @date 11-09-2026
 */

#include "server/server.h"
#include "config/config.h"
#include "server/database_manager.h"
#include "server/gossip_manager.h"
#include "server/heartbeat.h"
#include "utils/health/db_health_adapter.h"
#include "utils/health/heartbeat_health_adapter.h"
#include "utils/health/server_health_adapter.h"
#include "utils/logger/consoleLogger.h"
#include "utils/logger/logger.h"
#include <iostream>
#include <memory>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>

// get connections
ConnectionManager &Server::connections() { return connectionManager; }

// get gossip manager
GossipManager &Server::gossip() { return gossipManager; }

// get health monitor
HealthMonitor &Server::health() { return healthMonitor; }

// start the server
void Server::start() {
  static ConsoleLogger consoleLogger;
  Logger::getInstance().addLogger(&consoleLogger);

  // check if the server is already running
  if (running) {
    Logger::logInfo("Server", "Server is already running");
    return;
  }

  Logger::logInfo("Server", "Starting server"); // log the server starting

  // initialize winsock 2.2
  WSADATA data;
  if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
    Logger::logError("Server", "WSAStartup failed");
    return;
  }

  // start listening on the port
  if (!connectionManager.startListening(config::PORT)) {
    Logger::logError("Server", "Failed to start listening on port " + std::to_string(config::PORT));
    WSACleanup();
    return;
  }

  running = true; // set the server to running

  // start heartbeat thread
  heartbeat.start();

  // create gossip manager thread
  connectionManager.setGossip(&gossipManager);
  gossipManager.start();

  // start the accept loop
  acceptThread = std::thread([this] { connectionManager.acceptLoop(); });

  // add the health checks to the health monitor
  healthMonitor.add(std::make_unique<ServerHealthAdapter>(*this));        // server health
  healthMonitor.add(std::make_unique<HeartbeatHealthAdapter>(heartbeat)); // heartbeat health
  healthMonitor.add(
      std::make_unique<DbHealthAdapter>(DatabaseManager::getInstance())); // database health

  Logger::logInfo("Server", "Started on port " + std::to_string(config::PORT) + " with " +
                                std::to_string(config::THREAD_COUNT) +
                                " worker threads"); // log the server started
}

// stop the server
void Server::stop() {
  // check if the server is not running
  if (!running) {
    Logger::logInfo("Server", "Server is not running");
    return;
  }

  // set the server to not running
  running = false;

  // stop all threads
  heartbeat.stop();
  gossipManager.stop();
  connectionManager.setGossip(nullptr);
  connectionManager.stopListening();
  if (acceptThread.joinable()) {
    acceptThread.join();
  }
  threadPool.shutdown();

  // clean up winsock
  WSACleanup();
  Logger::logInfo("Server", "Server stopped");
}

// dashboard of the server
void Server::dashboard() {
  std::cout << "--------------------------------" << std::endl;
  std::cout << "Server dashboard" << std::endl;
  std::cout << "--------------------------------" << std::endl;
  std::cout << "Node id: " << config::NODE_ID << std::endl;
  std::cout << "Port: " << config::PORT << " Peer port: " << config::PEER_PORT
            << " Thread count: " << config::THREAD_COUNT << std::endl;
  std::cout << "Database path: " << config::DB_PATH << std::endl;
  std::cout << "Gossip seeds: ";
  if (config::PEERS.empty()) {
    std::cout << "(none)";
  } else {
    for (std::size_t i = 0; i < config::PEERS.size(); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      std::cout << config::PEERS[i];
    }
  }
  std::cout << std::endl;
  std::cout << "Client endpoints: ";
  {
    const auto live = gossipManager.getClientPeers();
    if (live.empty()) {
      std::cout << "(none)";
    } else {
      bool first = true;
      for (const auto &entry : live) {
        if (!first) {
          std::cout << ", ";
        }
        first = false;
        std::cout << entry.second.nodeId << "=" << entry.second.host << ":" << entry.second.port;
      }
    }
  }
  std::cout << std::endl;
  std::cout << "Listening: " << (connectionManager.isListening() ? "yes" : "no") << std::endl;

  std::cout << "Health:" << std::endl;
  HealthStatus overallStatus = HealthStatus::Up;
  std::string overallDetail;
  for (const auto &report : healthMonitor.checkAll()) {
    std::cout << "  " << report.name << ": " << report.statusToString();
    if (!report.detail.empty()) {
      std::cout << " (" << report.detail << ")";
    }
    std::cout << std::endl;
    if (report.status == HealthStatus::Down) {
      overallStatus = HealthStatus::Down;
      if (!overallDetail.empty()) {
        overallDetail += "; ";
      }
      overallDetail += report.name + ":Down";
    } else if (report.status == HealthStatus::Degraded && overallStatus != HealthStatus::Down) {
      overallStatus = HealthStatus::Degraded;
    }
  }
  std::cout << "Overall: " << healthStatusToString(overallStatus);
  if (!overallDetail.empty()) {
    std::cout << " (" << overallDetail << ")";
  }
  std::cout << std::endl;

  std::cout << "--------------------------------\n" << std::endl;
}

// check if the server is alive
bool Server::isAlive() { return running; }

// destructor
Server::~Server() { stop(); }