/**
 * Client class
 *
 * @brief Wires together the client components and runs a basic login flow.
 * @date 13-09-2026
 */

#include "client/client.h"
#include "client/console_ui.h"
#include "client/gui/pages/main_page.h"
#include "client/packet_builder.h"
#include "client/packet_handler.h"
#include "utils/RESPONSE_CODES.h"
#include "utils/models/logger.h"
#include "utils/models/packet.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <optional>
#include <string>

namespace {
std::atomic<std::uint64_t> next_client_id{0};
} // namespace

// waiting for a packet of a specific type
std::optional<Packet> Client::waitFor(Packet::PacketType expected) {
  while (true) {
    auto response = network.receivePacket();
    if (!response) {
      Logger::logError("Client " + id, "Connection lost while waiting for response");
      return std::nullopt;
    }
    if (response->type == expected) {
      return response;
    }
    // ignore unexpected queued types (heartbeats/pushes are handled in Network)
  }
}

// apply a room directory payload (login uses room field; pushes use message)
void Client::applyRoomDirectory(const std::string &encoded) {
  try {
    auto rooms = Room::deserializeList(encoded);
    const std::size_t count = rooms.size();
    state.setRooms(std::move(rooms));
    Logger::logInfo("Client " + id, "Room directory updated (" + std::to_string(count) + ")");
  } catch (const std::exception &e) {
    Logger::logWarning("Client " + id, std::string("Bad room directory: ") + e.what());
  }
}

// apply the room list push
void Client::applyRoomListPush(const Packet &packet) {
  if (!packet.message.empty() && packet.message.rfind("room(", 0) == 0) {
    applyRoomDirectory(packet.message);
  } else if (!packet.room.empty() && packet.room.rfind("room(", 0) == 0) {
    applyRoomDirectory(packet.room);
  }

  if (!packet.room.empty() && packet.room.rfind("room(", 0) != 0) {
    if (packet.room == "Lobby") {
      state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
    } else {
      state.currentRoom = Room(0, packet.room);
    }
  }

  const std::string &userPayload =
      (packet.receiver.rfind("user(", 0) == 0) ? packet.receiver : packet.sender;
  if (userPayload.rfind("user(", 0) == 0) {
    try {
      state.user = User::deserialize(userPayload);
    } catch (const std::exception &e) {
      Logger::logWarning("Client " + id, std::string("Bad connect user: ") + e.what());
    }
  }

  if (!state.getRooms().empty()) {
    std::lock_guard<std::mutex> lock(welcomeMutex);
    welcomeReceived = true;
  }
  welcomeCv.notify_all();
}

// start the client
bool Client::start() {
  id = std::to_string(++next_client_id); // create a unique id for the client

  {
    std::lock_guard<std::mutex> lock(welcomeMutex);
    welcomeReceived = false;
  }

  network.setPushHandler([this](const Packet &packet) {
    if (packet.type == Packet::PacketType::ROOM_LIST) {
      applyRoomListPush(packet);
    } else if (packet.type == Packet::PacketType::MESSAGE) {
      enqueueChatPush(packet);
    }
  });

  // connect to the server
  if (!network.connect()) {
    Logger::logError("Client " + id, "Failed to connect to the server");
    return false;
  }

  // wait for a non-empty room directory before opening UI
  {
    std::unique_lock<std::mutex> lock(welcomeMutex);
    welcomeCv.wait_for(lock, std::chrono::seconds(3),
                       [this] { return welcomeReceived || !network.isConnected(); });
  }

  if (state.getRooms().empty()) {
    Logger::logWarning(
        "Client " + id,
        "No room directory from server (using Lobby/General fallback). "
        "Connect to the server --port (client TCP), not --peer-port (gossip). "
        "Example: client --port 5559 when server uses --port 5559 --peer-port 5560.");
    state.setRooms({
        Room(1, "Lobby", Room::RoomType::LOBBY),
        Room(2, "General", Room::RoomType::OTHER),
    });
  }

  // match server session default until the welcome ROOM_LIST arrives
  if (!state.currentRoom) {
    state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
  }

  return true;
}

// enqueue a chat push for the UI
void Client::enqueueChatPush(const Packet &packet) {
  // only surface messages for the room the client is currently in
  if (state.currentRoom && !packet.room.empty() &&
      packet.room != state.currentRoom->getName()) {
    return;
  }
  std::lock_guard<std::mutex> lock(chatMutex);
  pendingChat.push_back(
      ChatLine{packet.sender, packet.message,
               packet.timestamp != 0 ? packet.timestamp
                                     : static_cast<std::uint64_t>(std::time(nullptr))});
}

// drain chat pushes received on the network thread
std::vector<ChatLine> Client::takePendingChatMessages() {
  std::lock_guard<std::mutex> lock(chatMutex);
  std::vector<ChatLine> out;
  out.swap(pendingChat);
  return out;
}

// stop the client
void Client::stop() { network.disconnect(); }

// check if the client is alive
bool Client::isAlive() const { return network.isConnected(); }

// join a room by name (requires login). Empty = success; otherwise error text.
std::string Client::joinRoom(std::string_view roomName) {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Join failed: " + error);
    return error;
  }
  if (!state.user) {
    const std::string error = "No user identity yet — wait for the server welcome.";
    Logger::logError("Client " + id, "Join failed: " + error);
    return error;
  }

  const std::string name(roomName);
  if (name.empty()) {
    const std::string error = "Room name cannot be empty.";
    Logger::logError("Client " + id, "Join failed: " + error);
    return error;
  }

  Packet joinPacket;
  try {
    joinPacket = PacketBuilder::buildJoinRoom(state.user->getUsername(), name);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Join failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(joinPacket)) {
    const std::string error = "Failed to send join request.";
    Logger::logError("Client " + id, "Join failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::ROOM_JOIN);
  if (!response) {
    const std::string error = "Connection lost while joining.";
    Logger::logError("Client " + id, "Join failed: " + error);
    return error;
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Join failed.") : response->message;
    Logger::logError("Client " + id, "Join failed: " + error);
    return error;
  }

  if (response->room == "Lobby") {
    state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
  } else {
    state.currentRoom = Room(0, response->room);
  }
  Logger::logInfo("Client " + id, "Joined room " + response->room);
  return {};
}

// leave the current room (back to Lobby). Empty = success; otherwise error text.
std::string Client::leaveRoom() {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Leave failed: " + error);
    return error;
  }
  if (!state.user) {
    const std::string error = "No user identity yet — wait for the server welcome.";
    Logger::logError("Client " + id, "Leave failed: " + error);
    return error;
  }
  if (!state.currentRoom) {
    const std::string error = "Not in a room.";
    Logger::logError("Client " + id, "Leave failed: " + error);
    return error;
  }
  if (state.currentRoom->getType() == Room::RoomType::LOBBY ||
      state.currentRoom->getName() == "Lobby") {
    const std::string error = "Cannot leave the Lobby.";
    Logger::logError("Client " + id, "Leave failed: " + error);
    return error;
  }

  const std::string roomName = state.currentRoom->getName();
  Packet leavePacket;
  try {
    leavePacket = PacketBuilder::buildLeaveRoom(state.user->getUsername(), roomName);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Leave failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(leavePacket)) {
    const std::string error = "Failed to send leave request.";
    Logger::logError("Client " + id, "Leave failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::ROOM_LEAVE);
  if (!response) {
    const std::string error = "Connection lost while leaving.";
    Logger::logError("Client " + id, "Leave failed: " + error);
    return error;
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Leave failed.") : response->message;
    Logger::logError("Client " + id, "Leave failed: " + error);
    return error;
  }

  state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
  Logger::logInfo("Client " + id, "Left room " + roomName);
  return {};
}

// log in with username/password. Empty = success; otherwise error text.
std::string Client::login(std::string_view username, std::string_view password) {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Login failed: " + error);
    return error;
  }
  if (state.isLoggedIn()) {
    const std::string error = "Already logged in.";
    Logger::logError("Client " + id, "Login failed: " + error);
    return error;
  }

  Packet loginPacket;
  try {
    loginPacket = PacketBuilder::buildLogin(username, password);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Login failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(loginPacket)) {
    const std::string error = "Failed to send login request.";
    Logger::logError("Client " + id, "Login failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::LOGIN);
  if (!response) {
    const std::string error = "Connection lost while logging in.";
    Logger::logError("Client " + id, "Login failed: " + error);
    return error;
  }

  if (auto user = handler.handlePacket(*response)) {
    state.user = *user;
    state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
    applyRoomDirectory(response->room);
    Logger::logInfo("Client " + id, "Logged in as " + user->getUsername());
    return {};
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Login failed.") : response->message;
    Logger::logError("Client " + id, "Login failed: " + error);
    return error;
  }

  const std::string error = "Login failed: invalid user payload";
  Logger::logError("Client " + id, error);
  return error;
}

// register a new account (and log in). Empty = success; otherwise error text.
std::string Client::signUp(std::string_view username, std::string_view password,
                           std::string_view email) {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Sign up failed: " + error);
    return error;
  }
  if (state.isLoggedIn()) {
    const std::string error = "Already logged in.";
    Logger::logError("Client " + id, "Sign up failed: " + error);
    return error;
  }

  Packet registerPacket;
  try {
    registerPacket = PacketBuilder::buildRegister(username, password, email);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Sign up failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(registerPacket)) {
    const std::string error = "Failed to send sign-up request.";
    Logger::logError("Client " + id, "Sign up failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::REGISTER);
  if (!response) {
    const std::string error = "Connection lost while signing up.";
    Logger::logError("Client " + id, "Sign up failed: " + error);
    return error;
  }

  if (auto user = handler.handlePacket(*response)) {
    state.user = *user;
    state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
    applyRoomDirectory(response->room);
    Logger::logInfo("Client " + id, "Signed up as " + user->getUsername());
    return {};
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Sign up failed.") : response->message;
    Logger::logError("Client " + id, "Sign up failed: " + error);
    return error;
  }

  const std::string error = "Sign up failed: invalid user payload";
  Logger::logError("Client " + id, error);
  return error;
}

// log out (back to guest). Empty = success; otherwise error text.
std::string Client::logout() {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Logout failed: " + error);
    return error;
  }
  if (!state.isLoggedIn() || !state.user) {
    const std::string error = "Not logged in.";
    Logger::logError("Client " + id, "Logout failed: " + error);
    return error;
  }

  Packet logoutPacket;
  try {
    logoutPacket = PacketBuilder::buildLogout(*state.user);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Logout failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(logoutPacket)) {
    const std::string error = "Failed to send logout request.";
    Logger::logError("Client " + id, "Logout failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::LOGOUT);
  if (!response) {
    const std::string error = "Connection lost while logging out.";
    Logger::logError("Client " + id, "Logout failed: " + error);
    return error;
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Logout failed.") : response->message;
    Logger::logError("Client " + id, "Logout failed: " + error);
    return error;
  }

  // stay connected as a guest in Lobby; keep the room directory
  auto rooms = state.getRooms();
  state.clear();
  state.setRooms(std::move(rooms));
  state.currentRoom = Room(1, "Lobby", Room::RoomType::LOBBY);
  state.user = User::anonymousUser();
  clearPendingChatMessages();
  Logger::logInfo("Client " + id, "Logged out");
  return {};
}

// create a room (requires login). Empty = success; otherwise error text.
std::string Client::createRoom(std::string_view roomName, std::string_view privacy) {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Create room failed: " + error);
    return error;
  }
  if (!state.isLoggedIn() || !state.user) {
    const std::string error = "Log in to create a room.";
    Logger::logError("Client " + id, "Create room failed: " + error);
    return error;
  }

  Packet createPacket;
  try {
    createPacket = PacketBuilder::buildCreateRoom(state.user->getUsername(), roomName, privacy);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Create room failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(createPacket)) {
    const std::string error = "Failed to send create-room request.";
    Logger::logError("Client " + id, "Create room failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::ROOM_CREATE);
  if (!response) {
    const std::string error = "Connection lost while creating room.";
    Logger::logError("Client " + id, "Create room failed: " + error);
    return error;
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Create room failed.") : response->message;
    Logger::logError("Client " + id, "Create room failed: " + error);
    return error;
  }

  // directory push may already have updated rooms; ensure the new name is present
  if (!response->room.empty()) {
    auto rooms = state.getRooms();
    bool known = false;
    for (const Room &room : rooms) {
      if (room.getName() == response->room) {
        known = true;
        break;
      }
    }
    if (!known) {
      rooms.push_back(Room(0, response->room));
      state.setRooms(std::move(rooms));
    }
  }

  Logger::logInfo("Client " + id, "Room created: " + response->room);
  return {};
}

// invite a user to a private room (requires login). Empty = success; otherwise error text.
std::string Client::inviteToRoom(std::string_view roomName, std::string_view inviteeUsername) {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Invite failed: " + error);
    return error;
  }
  if (!state.isLoggedIn() || !state.user) {
    const std::string error = "Log in to invite users.";
    Logger::logError("Client " + id, "Invite failed: " + error);
    return error;
  }

  const std::string room =
      roomName.empty() && state.currentRoom ? state.currentRoom->getName() : std::string(roomName);

  Packet invitePacket;
  try {
    invitePacket =
        PacketBuilder::buildInviteToRoom(state.user->getUsername(), room, inviteeUsername);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Invite failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(invitePacket)) {
    const std::string error = "Failed to send invite request.";
    Logger::logError("Client " + id, "Invite failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::ROOM_INVITE);
  if (!response) {
    const std::string error = "Connection lost while inviting.";
    Logger::logError("Client " + id, "Invite failed: " + error);
    return error;
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Invite failed.") : response->message;
    Logger::logError("Client " + id, "Invite failed: " + error);
    return error;
  }

  Logger::logInfo("Client " + id, response->message.empty() ? "Invite sent" : response->message);
  return {};
}

// update profile (username and optional new password). Empty = success; otherwise error text.
std::string Client::updateProfile(std::string_view username, std::string_view newPassword,
                                  std::string_view email) {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Update profile failed: " + error);
    return error;
  }
  if (!state.isLoggedIn() || !state.user) {
    const std::string error = "Log in to update your profile.";
    Logger::logError("Client " + id, "Update profile failed: " + error);
    return error;
  }

  Packet updatePacket;
  try {
    updatePacket = PacketBuilder::buildUpdateUser(username, newPassword, email);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Update profile failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(updatePacket)) {
    const std::string error = "Failed to send update request.";
    Logger::logError("Client " + id, "Update profile failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::UPDATE_USER);
  if (!response) {
    const std::string error = "Connection lost while updating profile.";
    Logger::logError("Client " + id, "Update profile failed: " + error);
    return error;
  }

  if (auto user = handler.handlePacket(*response)) {
    state.user = *user;
    Logger::logInfo("Client " + id, "Profile updated: " + user->getUsername());
    return {};
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Update profile failed.") : response->message;
    Logger::logError("Client " + id, "Update profile failed: " + error);
    return error;
  }

  const std::string error = "Update profile failed: invalid user payload";
  Logger::logError("Client " + id, error);
  return error;
}

// send a chat message in the current room (guests OK; not persisted). Empty = success; otherwise error text.
std::string Client::sendMessage(std::string_view text) {
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Send failed: " + error);
    return error;
  }
  if (!state.user) {
    const std::string error = "No user identity yet — wait for the server welcome.";
    Logger::logError("Client " + id, "Send failed: " + error);
    return error;
  }

  const std::string body(text);
  if (body.empty()) {
    const std::string error = "Message cannot be empty.";
    Logger::logError("Client " + id, "Send failed: " + error);
    return error;
  }

  Packet messagePacket;
  try {
    messagePacket = PacketBuilder::buildMessage(state.user->getUsername(), body);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Send failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(messagePacket)) {
    const std::string error = "Failed to send message.";
    Logger::logError("Client " + id, "Send failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::MESSAGE);
  if (!response) {
    const std::string error = "Connection lost while sending.";
    Logger::logError("Client " + id, "Send failed: " + error);
    return error;
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Send failed.") : response->message;
    Logger::logError("Client " + id, "Send failed: " + error);
    return error;
  }

  Logger::logInfo("Client " + id, "Message sent successfully");
  return {};
}

namespace {
// parse encodeHistory lines: "[user][unix] text\n" (also accepts legacy "[user] text")
void parseHistoryPayload(const std::string &payload, std::vector<ChatLine> &out) {
  std::size_t start = 0;
  while (start < payload.size()) {
    std::size_t end = payload.find('\n', start);
    if (end == std::string::npos) {
      end = payload.size();
    }
    std::string line = payload.substr(start, end - start);
    start = end + 1;
    if (line.empty()) {
      continue;
    }
    if (line.front() == '[') {
      const std::size_t closeUser = line.find(']');
      if (closeUser != std::string::npos) {
        ChatLine row;
        row.author = line.substr(1, closeUser - 1);
        std::string rest = line.substr(closeUser + 1);
        if (!rest.empty() && rest.front() == '[') {
          const std::size_t closeTs = rest.find(']');
          if (closeTs != std::string::npos) {
            try {
              row.timestamp = static_cast<std::uint64_t>(std::stoull(rest.substr(1, closeTs - 1)));
            } catch (...) {
              row.timestamp = 0;
            }
            rest = rest.substr(closeTs + 1);
          }
        }
        if (!rest.empty() && rest.front() == ' ') {
          rest.erase(0, 1);
        }
        row.text = std::move(rest);
        out.push_back(std::move(row));
        continue;
      }
    }
    out.push_back(ChatLine{"system", std::move(line), 0});
  }
}
} // namespace

// load history for the current room into out. Empty = success; otherwise error.
std::string Client::loadMessageHistory(std::vector<ChatLine> &out) {
  out.clear();
  if (!isAlive()) {
    const std::string error = "Not connected to the server.";
    Logger::logError("Client " + id, "Load history failed: " + error);
    return error;
  }
  if (!state.user || !state.currentRoom) {
    const std::string error = "No room to load history for.";
    Logger::logError("Client " + id, "Load history failed: " + error);
    return error;
  }

  const std::string roomName = state.currentRoom->getName();
  Packet historyPacket;
  try {
    historyPacket = PacketBuilder::buildLoadMessageHistory(state.user->getUsername(), roomName);
  } catch (const std::invalid_argument &e) {
    Logger::logError("Client " + id, std::string("Load history failed: ") + e.what());
    return std::string(e.what());
  }

  if (!network.sendPacket(historyPacket)) {
    const std::string error = "Failed to request history.";
    Logger::logError("Client " + id, "Load history failed: " + error);
    return error;
  }

  auto response = waitFor(Packet::PacketType::LOAD_MESSAGE_HISTORY);
  if (!response) {
    const std::string error = "Connection lost while loading history.";
    Logger::logError("Client " + id, "Load history failed: " + error);
    return error;
  }

  if (response->responseCode != static_cast<int>(RESPONSE_CODES::SUCCESS)) {
    const std::string error =
        response->message.empty() ? std::string("Load history failed.") : response->message;
    Logger::logError("Client " + id, "Load history failed: " + error);
    return error;
  }

  parseHistoryPayload(response->message, out);
  return {};
}

// drop any queued chat pushes (e.g. when switching rooms)
void Client::clearPendingChatMessages() {
  std::lock_guard<std::mutex> lock(chatMutex);
  pendingChat.clear();
}

// showing the dashboard
void Client::showDashboard() {
  if (!state.isLoggedIn()) {
    // show home screen
    int answer = ConsoleUI::showHomeScreen();

    switch (answer) {
      // login option
    case 1: {
      std::optional<Packet> loginPacket = ConsoleUI::showLogin();
      if (!loginPacket) {
        return;
      }
      login(loginPacket->sender, loginPacket->message);
      break;
    }

    // register option
    case 2: {
      std::optional<Packet> registerPacket = ConsoleUI::showRegister();
      if (!registerPacket) {
        return;
      }
      signUp(registerPacket->sender, registerPacket->message, registerPacket->room);
      break;
    }

    // exit option
    case 3: {
      stop();
      break;
    }
    default:
      break;
    }
  } else {
    // show user dashboard
    int answer = ConsoleUI::showUserDashboard(state);

    switch (answer) {
      // update profile option
    case 1: {
      std::optional<Packet> updatePacket = ConsoleUI::showUpdateProfile(*state.user);
      if (!updatePacket) {
        break;
      }
      updateProfile(updatePacket->sender, updatePacket->message, updatePacket->room);
      break;
    }

    // join room option
    case 2: {
      std::optional<Packet> joinRoomPacket = ConsoleUI::showJoinRoom(state);
      if (!joinRoomPacket) {
        break;
      }
      joinRoom(joinRoomPacket->room);
      break;
    }

    // leave room option
    case 3: {
      leaveRoom();
      break;
    }

    // create message option
    case 4: {
      std::optional<Packet> createMessagePacket = ConsoleUI::showCreateMessage(*state.user);
      if (!createMessagePacket) {
        break;
      }
      sendMessage(createMessagePacket->message);
      break;
    }

    // create room option
    case 5: {
      std::optional<Packet> createRoomPacket = ConsoleUI::showCreateRoom(*state.user);
      if (!createRoomPacket) {
        break;
      }
      createRoom(createRoomPacket->room, createRoomPacket->message);
      break;
    }

    // load message history option
    case 6: {
      std::vector<ChatLine> lines;
      const std::string error = loadMessageHistory(lines);
      if (!error.empty()) {
        break;
      }
      std::cout << ">> History for "
                << (state.currentRoom ? state.currentRoom->getName() : std::string("?")) << ":\n";
      if (lines.empty()) {
        std::cout << "   (no messages)\n";
      } else {
        for (const ChatLine &line : lines) {
          std::cout << "   [" << line.author << "] " << line.text << '\n';
        }
      }
      break;
    }

    // invite to room option
    case 7: {
      std::optional<Packet> invitePacket = ConsoleUI::showInviteToRoom(state);
      if (!invitePacket) {
        break;
      }
      inviteToRoom(invitePacket->room, invitePacket->message);
      break;
    }

    // logout option
    case 8: {
      logout();
      break;
    }
    default:
      break;
    }
  }
}

// showing the Qt dashboard (stub Page — no ConsoleUI loop)
int Client::showDashboard_2(int argc, char *argv[]) { return MainPage::run(argc, argv, *this); }