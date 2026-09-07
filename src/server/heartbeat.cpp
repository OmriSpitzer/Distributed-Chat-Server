/**
 * Heartbeat class
 *
 * @brief Periodic keepalive: ping live clients and drop silent sockets.
 * @date 07-09-2026
 */

#include "server/heartbeat.h"
#include "config/config.h"
#include "server/connection_manager.h"
#include "server/packet_processor.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"

// constructor
Heartbeat::Heartbeat(ConnectionManager &connections) : connections(connections) {}

// destructor
Heartbeat::~Heartbeat() { stop(); }

// start the heartbeat
void Heartbeat::start() {
  // check if the heartbeat thread is already running
  if (heartbeat_thread.joinable()) {
    return;
  }

  stopped = false;

  // start the heartbeat thread
  heartbeat_thread = std::thread([this]() {
    // lock the heartbeat mutex
    std::unique_lock lock(heartbeat_mutex);

    while (!stopped) {
      if (condition_variable.wait_for(lock, std::chrono::milliseconds(config::HEARTBEAT_INTERVAL),
                                      [this] { return stopped.load(); })) {
        break;
      }

      // in mutex create a new packet and process it
      lock.unlock();

      auto sessions = connections.getSessions();
      Packet ping("server", "", Packet::PacketType::HEARTBEAT, "", "ping");
      for (const auto &entry : sessions) {
        const ClientSession &session = *entry.second;
        if (!session.isAlive()) {
          connections.closeClient(session.getSocket());
          continue;
        }
        connections.sendPacket(session.getSocket(), ping);
      }

      Packet tick("heartbeat", "server", Packet::PacketType::HEARTBEAT, "", "ping");
      Packet summary = PacketProcessor::processHeartbeatPacket(tick, connections);
      Logger::logHeartbeat("Heartbeat", summary.message);

      lock.lock();
    }
  });
  Logger::logInfo("Heartbeat", "Started");
}

// stop the heartbeat
void Heartbeat::stop() {
  // stop the heartbeat
  stopped = true;
  condition_variable.notify_all();

  // join the heartbeat thread
  if (heartbeat_thread.joinable()) {
    heartbeat_thread.join();
  }

  Logger::logInfo("Heartbeat", "Stopped");
}