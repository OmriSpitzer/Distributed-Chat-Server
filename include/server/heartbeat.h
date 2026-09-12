/**
 * Heartbeat header file class
 *
 * @date 12-09-2026
 */

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

class ConnectionManager;

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

  // delete copy and move
  Heartbeat(const Heartbeat &) = delete;
  Heartbeat &operator=(const Heartbeat &) = delete;
  Heartbeat(Heartbeat &&) = delete;
  Heartbeat &operator=(Heartbeat &&) = delete;

private:
  // main heartbeat loop
  void run();

  // snapshot pinging all sessions
  void pingOnce();

  ConnectionManager &connections;   // connections
  std::thread heartbeat_thread;     // heartbeat thread
  std::mutex lifecycle_mutex;       // serializes start / stop / join
  std::mutex wait_mutex;            // heartbeat wait mutex
  std::condition_variable stop_cv;  // interruptible sleep
  std::atomic<bool> stopped{true};  // request to stop the worker
  std::atomic<bool> running{false}; // worker is alive (or start is in progress)
};
