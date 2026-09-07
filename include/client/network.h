/**
 * Network header file class
 *
 * @date 07-09-2026
 */
#pragma once
#include "utils/models/packet.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <thread>

class Network {
public:
  // constructor
  Network() = default;

  // destructor
  ~Network();

  // delete copy constructor and assignment operator
  Network(const Network &) = delete;
  Network &operator=(const Network &) = delete;

  // connecting to the main server
  bool connect();

  // disconnecting from the main server
  void disconnect();

  // sending a packet to the main server
  bool sendPacket(const Packet &packet, std::string_view message);

  // receiving a packet from the server
  std::optional<Packet> receivePacket();

  // check if the network is connected
  bool isConnected() const;

private:
  int clientSocket = -1;              // connected TCP socket
  std::atomic<bool> connected{false}; // whether the network is connected
  bool winsockStarted = false;        // whether this instance called WSAStartup
  mutable std::mutex mutex;           // guards send, queue, and socket id
  std::thread readerThread;           // reads the socket
  std::queue<Packet> incoming;        // packets waiting for receivePacket
  std::condition_variable incomingCv; // wait for a queued packet

  // read loop: pong heartbeats, queue everything else
  void readerLoop();
};
