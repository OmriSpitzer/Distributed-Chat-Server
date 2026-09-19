/**
 * Client header file class
 *
 * @date 13-09-2026
 */

#pragma once
#include "client/client_state.h"
#include "client/network.h"
#include "client/packet_handler.h"
#include "utils/models/packet.h"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ChatLine {
  std::string author;
  std::string text;
  std::uint64_t timestamp{0}; // unix seconds; 0 = unknown
};

class Client {
public:
  // starting the client
  bool start();

  // stopping the client
  void stop();

  // check if the client is alive
  bool isAlive() const;

  // showing the dashboard (console)
  void showDashboard();

  // showing the dashboard (Qt stub page)
  int showDashboard_2(int argc, char *argv[]);

  // get the client state
  ClientState &getState() { return state; }
  const ClientState &getState() const { return state; }

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
  std::string id;        // client id
  Network network;       // network class
  PacketHandler handler; // packet handler class
  ClientState state;     // client state class

  std::mutex welcomeMutex;           // guards welcomeReceived
  std::condition_variable welcomeCv; // signaled when ROOM_LIST applied
  bool welcomeReceived{false};       // first ROOM_LIST (connect snapshot) seen

  std::mutex chatMutex;           // guards pendingChat
  std::vector<ChatLine> pendingChat;

  // waiting for a packet of a specific type
  std::optional<Packet> waitFor(Packet::PacketType expected);

  // cache room directory from login reply or ROOM_LIST push
  void applyRoomDirectory(const std::string &encoded);

  // apply the room list push
  void applyRoomListPush(const Packet &packet);

  // enqueue a chat push for the UI
  void enqueueChatPush(const Packet &packet);
};
