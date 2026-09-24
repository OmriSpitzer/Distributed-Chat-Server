/**
 * Heartbeat class implementation file
 *
 * @brief Periodic keepalive: ping live clients and drop silent sockets
 *
 * Heartbeat class with fields: connections, lifecycle_mutex, stop_cv, heartbeat_thread, running,
 * stopped
 * Used for managing the heartbeat lifecycle
 * Session format: [<username>,<room name>]
 * Heartbeat format: sessions: <session1> <session2> ... total: <number of sessions>
 * @date 12-09-2026
 */

#include "server/heartbeat.h"
#include "config/config.h"
#include "server/client_session.h"
#include "server/connection_manager.h"
#include "utils/logger/logger.h"
#include "utils/models/packet.h"
#include <chrono>
#include <exception>
#include <string>

// constructor
Heartbeat::Heartbeat(ConnectionManager &connections) : connections(connections) {}

// destructor
Heartbeat::~Heartbeat() { stop(); }

// start the heartbeat
void Heartbeat::start() {
  std::lock_guard lifecycle(lifecycle_mutex);

  // check if already running
  if (running) {
    return;
  }

  // join a worker that exited without stop()
  if (heartbeat_thread.joinable()) {
    heartbeat_thread.join();
  }

  // set flags
  stopped = false;
  running = true;

  // start the heartbeat thread
  try {
    heartbeat_thread = std::thread([this] {
      run();
      running = false;
    });
  } catch (const std::exception &ex) {
    running = false;
    stopped = true;
    Logger::logError("Heartbeat", ex.what());
    return;
  }

  // log the heartbeat started
  Logger::logInfo("Heartbeat", "Started");
}

// stop the heartbeat
void Heartbeat::stop() {
  std::lock_guard lifecycle(lifecycle_mutex);

  // set stopped flag
  stopped = true;

  // notify all threads waiting on the condition variable
  stop_cv.notify_all();

  // check if the heartbeat thread is joinable
  if (!heartbeat_thread.joinable()) {
    running = false;
    return;
  }

  // join the heartbeat thread
  heartbeat_thread.join();
  running = false;

  // log the heartbeat stopped
  Logger::logInfo("Heartbeat", "Stopped");
}

// main heartbeat loop
void Heartbeat::run() {
  try {
    while (!stopped) {
      // wait for the heartbeat interval or the stopped flag to be set
      {
        std::unique_lock lock(wait_mutex);
        if (stop_cv.wait_for(lock, std::chrono::milliseconds(config::HEARTBEAT_INTERVAL),
                             [this] { return stopped.load(); })) {
          return;
        }
      }

      try {
        pingOnce(); // ping once to all sessions
      } catch (const std::exception &ex) {
        Logger::logError("Heartbeat", ex.what());
      } catch (...) {
        Logger::logError("Heartbeat", "Unknown error during ping");
      }
    }
  } catch (const std::exception &ex) {
    Logger::logError("Heartbeat", ex.what());
  } catch (...) {
    Logger::logError("Heartbeat", "Worker exited unexpectedly");
  }
}

// snapshot pinging all sessions
void Heartbeat::pingOnce() {
  auto sessions = connections.getSessions();                            // get all sessions
  Packet ping("server", "", Packet::PacketType::HEARTBEAT, "", "ping"); // create the ping packet

  // build the message
  std::string message = "sessions: ";

  // ping each session in connections
  for (const auto &entry : sessions) {
    if (stopped) {
      break;
    }

    // get the session
    const ClientSession &session = *entry.second;
    message += "[" + session.getUser().getUsername() + "," + session.getRoom().getName() + "] ";

    // if the session is closed, skip it
    if (session.isClosed()) {
      continue;
    }

    // close the client if it is not alive or if the packet is not sent
    if (!session.isAlive() || !connections.sendPacket(session.getSocket(), ping)) {
      connections.closeClient(session.getSocket());
    }
  }

  // log the heartbeat
  message += "total: " + std::to_string(sessions.size());
  Logger::logHeartbeat("Heartbeat", message);
}
