/**
 * Network header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "utils/models/packet.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string_view>
#include <thread>
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

class Network {
public:
  using PushHandler = std::function<void(const Packet &)>;

  // constructor
  Network() = default;

  // destructor
  ~Network();

  // delete copy constructor and assignment operator
  Network(const Network &) = delete;
  Network &operator=(const Network &) = delete;

  // connecting to config::SERVER_HOST:PORT
  bool connect();

  // connecting to a specific chat endpoint
  bool connect(std::string_view host, std::uint16_t port);

  // disconnecting from the main server
  void disconnect();

  // sending a packet to the main server
  bool sendPacket(const Packet &packet);

  // receiving a packet from the server
  std::optional<Packet> receivePacket();

  // wait up to timeout for a queued packet; nullopt on timeout or disconnect
  std::optional<Packet> receivePacketFor(std::chrono::milliseconds timeout);

  // check if the network is connected
  bool isConnected() const;

  // true when the socket is up and no packet has arrived within HEARTBEAT_TIMEOUT
  bool heartbeatMissed() const;

  // optional handler for unsolicited pushes (ROOM_LIST, etc.)
  void setPushHandler(PushHandler handler);

  // fired once when the socket drops (peer close or failed send)
  void setLinkDownHandler(std::function<void()> handler);

private:
  SOCKET clientSocket = INVALID_SOCKET; // connected TCP socket
  std::atomic<bool> connected{false};   // whether the network is connected
  bool winsockStarted = false;          // whether this instance called WSAStartup
  mutable std::mutex mutex;             // guards send, queue, and socket id
  std::thread readerThread;             // reads the socket
  std::queue<Packet> incoming;            // packets waiting for receivePacket
  std::condition_variable incomingCv;     // wait for a queued packet
  PushHandler pushHandler;                // optional push callback
  std::function<void()> linkDownHandler;  // optional socket-down callback
  std::atomic<std::int64_t> lastInboundMs{0}; // steady-clock ms of the last inbound packet

  // read loop: pong heartbeats, apply pushes, queue everything else
  void readerLoop();

  // stamp now as the last time the peer sent a packet
  void noteInbound();

  // mark the socket down and wake the failover loop once
  void markLinkDown();
};
