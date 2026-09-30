/**
 * Client header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "client/appearance.h"
#include "client/client_state.h"
#include "client/endpoint_ring.h"
#include "client/network.h"
#include "client/packet_handler.h"
#include "utils/health/health_monitor.h"
#include "utils/models/packet.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

struct ChatLine {
  std::string author;
  std::string text;
  std::uint64_t timestamp{0}; // unix seconds; 0 = unknown
};

class Client {
public:
  using Theme = ChatLook::Theme;
  using Density = ChatLook::Density;

  // joins the failover thread
  ~Client();

  // starting the client
  bool start();

  // stopping the client
  void stop();

  // check if the client is alive (socket up and heartbeat fresh)
  bool isAlive() const;

  // rolled-up client health (link adapter registered in start)
  HealthMonitor &health();

  // true while the failover supervisor is running
  bool isRunning() const;

  // true from a live-link drop until the next dial succeeds
  bool isReconnecting() const;

  // how many times a live link has dropped since start
  int reconnectCount() const;

  // showing the dashboard (console)
  void showDashboard();

  // showing the dashboard (Qt stub page)
  int showDashboard_2(int argc, char *argv[]);

  // get the client state
  ClientState &getState() { return state; }
  const ClientState &getState() const { return state; }

  // local dashboard look (not sent on the wire, not cleared on logout)
  Theme theme() const { return appearanceTheme; }
  Density density() const { return appearanceDensity; }
  void toggleTheme();
  void toggleDensity();

  // join a room by name (guests OK for public rooms). Empty = success; otherwise error text.
  std::string joinRoom(std::string_view roomName);

  // leave the current room (back to Lobby). Empty = success; otherwise error text.
  std::string leaveRoom();

  // log in with username/password. Empty = success; otherwise error text.
  std::string login(std::string_view username, std::string_view password);

  // register a new account (and log in). Empty = success; otherwise error text.
  std::string signUp(std::string_view username, std::string_view password, std::string_view email);

  // log out (back to guest). Empty = success; otherwise error text.
  std::string logout();

  // create a room (requires login). Empty = success; otherwise error text.
  std::string createRoom(std::string_view roomName, std::string_view privacy);

  // invite a user to a private room (requires login). Empty = success; otherwise error text.
  std::string inviteToRoom(std::string_view roomName, std::string_view inviteeUsername);

  // kick a user from a room (ADMIN, or room creator). Empty = success; otherwise error text.
  std::string kickFromRoom(std::string_view roomName, std::string_view targetUsername);

  // delete a room (ADMIN only; not Lobby / General). Empty = success; otherwise error text.
  std::string deleteRoom(std::string_view roomName);

  // update profile (username and optional new password). Empty = success; otherwise error text.
  std::string updateProfile(std::string_view username, std::string_view newPassword,
                            std::string_view email);

  // send a chat message in the current room (guests OK). Empty = success; otherwise error text.
  std::string sendMessage(std::string_view text);

  // load history for the current room into out. Empty = success; otherwise error.
  std::string loadMessageHistory(std::vector<ChatLine> &out);

  // drain chat pushes received on the network thread
  std::vector<ChatLine> takePendingChatMessages();

  // drop any queued chat pushes (e.g. when switching rooms)
  void clearPendingChatMessages();

private:
  std::string id;                                  // client id
  Network network;                                 // network class
  PacketHandler handler;                           // packet handler class
  ClientState state;                               // client state class
  Theme appearanceTheme{Theme::Light};             // theme appearance
  Density appearanceDensity{Density::Comfortable}; // density appearance

  std::mutex welcomeMutex;           // guards welcomeReceived
  std::condition_variable welcomeCv; // signaled when ROOM_LIST applied
  bool welcomeReceived{false};       // first ROOM_LIST (connect snapshot) seen

  std::mutex chatMutex;              // guards pendingChat
  std::vector<ChatLine> pendingChat; // queued chat messages

  EndpointRing ring;                          // round-robin failover list
  HealthMonitor healthMonitor;                // rolls up the client adapter
  bool healthWired{false};                    // adapter registered once
  std::thread supervisor;                     // dials and watches the link
  std::atomic<bool> stopRequested{false};     // stop() asked the supervisor to exit
  std::atomic<bool> supervisorRunning{false}; // supervisor thread is inside its loop
  std::atomic<bool> reconnecting{false};      // supervisor is dialing after a live link dropped
  std::atomic<int> reconnects{0};             // live-link drops since start
  std::atomic<bool> resumePending{false};     // send RECONNECT after the next successful dial
  std::mutex waitMutex;                       // guards the failover wait
  std::condition_variable waitCv;             // wakes sleep on stop or link down
  std::mt19937 rng{std::random_device{}()};   // jitter source

  // waiting for a packet of a specific type
  std::optional<Packet> waitFor(Packet::PacketType expected);

  // same wait, but give up when the timeout elapses
  std::optional<Packet> waitFor(Packet::PacketType expected, std::chrono::milliseconds timeout);

  // after a hop, send RECONNECT for the in-memory user and room
  void resumeSession();

  // rebuild the ring: directory-up endpoints, then the static seed
  void rebuildEndpoints();

  // dial the ring until stop or the attempt budget is spent
  void supervisorLoop();

  // socket up, heartbeat fresh, and HealthMonitor not Down
  bool linkHealthy();

  // mark a hop and log "reconnecting..." once per drop
  void markReconnecting();

  // advance the ring, sleep with backoff + jitter; false when the budget or stop ends the loop
  bool failAndWait(int &attempt);

  // sleep that returns false when stop() was requested
  bool sleepFor(std::chrono::milliseconds delay);

  // cache room directory from login reply or ROOM_LIST push
  void applyRoomDirectory(const std::string &encoded);

  // apply the room list push
  void applyRoomListPush(const Packet &packet);

  // apply SERVER_DIRECTORY push (failover hints)
  void applyServerDirectoryPush(const Packet &packet);

  // enqueue a chat push for the UI
  void enqueueChatPush(const Packet &packet);
};
