/**
 * WebConnection
 *
 * @brief WebSocket listen thread owned by Server. Binary frames carry one framed Packet.
 * Sessions live in ConnectionManager, same as TCP clients.
 * @date 29-09-2026
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

class ConnectionManager;

namespace web_connection {

enum class WsOpcode : std::uint8_t {
  Continuation = 0x0,
  Text = 0x1,
  Binary = 0x2,
  Close = 0x8,
  Ping = 0x9,
  Pong = 0xA,
};

struct FrameDecode {
  enum class Status { Ok, Incomplete, Error };

  Status status{Status::Error};
  std::size_t consumed{0};
  WsOpcode opcode{WsOpcode::Binary};
  std::string payload;
};

std::string acceptValue(std::string_view clientKey);

std::string encodeFrame(WsOpcode opcode, std::string_view payload, bool masked,
                        std::uint32_t maskKey = 0);

FrameDecode decodeFrame(std::string_view bytes);

std::string upgradeResponse(std::string_view httpRequest);

// Browser listen socket. Server runs acceptLoop on its own thread.
class WebConnection {
public:
  explicit WebConnection(ConnectionManager &connections);
  ~WebConnection();

  WebConnection(const WebConnection &) = delete;
  WebConnection &operator=(const WebConnection &) = delete;

  bool startListening(std::uint16_t port);
  void stopListening();
  void acceptLoop();
  void joinClientThreads();

  bool isListening() const;
  std::uint16_t port() const;

private:
  void handleBrowser(SOCKET browser);
  bool track(SOCKET socket);
  void untrack(SOCKET socket);

  ConnectionManager &connections_;
  std::uint16_t listenPort_{0};
  SOCKET listeningSocket_{INVALID_SOCKET};
  std::atomic<bool> listening_{false};
  std::mutex threadsMutex_;
  std::vector<std::thread> clientThreads_;
  std::mutex socketsMutex_;
  std::vector<SOCKET> liveSockets_;
};

} // namespace web_connection
