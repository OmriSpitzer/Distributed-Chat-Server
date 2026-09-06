/**
 * Heartbeat header file class
 *
 * @date 06-09-2026
 */

#pragma once
#include "server/connection_manager.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class Heartbeat {
public:
  // constructor
  explicit Heartbeat(ConnectionManager &connections);

  // destructor
  ~Heartbeat();

  // start the heartbeat
  void start();

  // stop the heartbeat
  void stop();

  // delete copy constructor and assignment operator
  Heartbeat(const Heartbeat &) = delete;
  Heartbeat &operator=(const Heartbeat &) = delete;

private:
  ConnectionManager &connections;             // connections
  std::thread heartbeat_thread;               // heartbeat thread
  std::mutex heartbeat_mutex;                 // heartbeat mutex
  std::condition_variable condition_variable; // heartbeat condition variable
  std::atomic<bool> stopped{true};            // stopped
  static constexpr int INTERVAL = 5000;       // interval in milliseconds
};