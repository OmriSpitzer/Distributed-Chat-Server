/**
 * WebConnection sidecar
 *
 * @brief WebSocket upgrade and binary frames. Owned by Server; sessions stay in ConnectionManager.
 * @date 29-09-2026
 */

#include "server/web_connection.h"
#include "server/connection_manager.h"
#include "utils/logger/logger.h"
#include "utils/models/packet.h"
#include "utils/serializer.h"
#include "utils/socket_io.h"
#include <array>
#include <cctype>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace web_connection {
namespace {

constexpr std::string_view kWebsocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr std::size_t kMaxHttpHeaderBytes = 8192;

// rotate left
std::uint32_t rol(std::uint32_t value, int bits) {
  return (value << bits) | (value >> (32 - bits));
}

// SHA-1 digest, 20 bytes
std::array<unsigned char, 20> sha1(std::string_view data) {
  std::uint32_t h0 = 0x67452301;
  std::uint32_t h1 = 0xEFCDAB89;
  std::uint32_t h2 = 0x98BADCFE;
  std::uint32_t h3 = 0x10325476;
  std::uint32_t h4 = 0xC3D2E1F0;

  std::vector<unsigned char> message(data.begin(), data.end());
  const std::uint64_t bitLength = static_cast<std::uint64_t>(data.size()) * 8;
  message.push_back(0x80);
  while ((message.size() % 64) != 56) {
    message.push_back(0);
  }
  for (int shift = 56; shift >= 0; shift -= 8) {
    message.push_back(static_cast<unsigned char>((bitLength >> shift) & 0xFF));
  }

  for (std::size_t offset = 0; offset < message.size(); offset += 64) {
    std::uint32_t words[80];
    for (int i = 0; i < 16; ++i) {
      const std::size_t at = offset + static_cast<std::size_t>(i) * 4;
      words[i] = (static_cast<std::uint32_t>(message[at]) << 24) |
                 (static_cast<std::uint32_t>(message[at + 1]) << 16) |
                 (static_cast<std::uint32_t>(message[at + 2]) << 8) |
                 static_cast<std::uint32_t>(message[at + 3]);
    }
    for (int i = 16; i < 80; ++i) {
      words[i] = rol(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
    }

    std::uint32_t a = h0;
    std::uint32_t b = h1;
    std::uint32_t c = h2;
    std::uint32_t d = h3;
    std::uint32_t e = h4;
    for (int i = 0; i < 80; ++i) {
      std::uint32_t f = 0;
      std::uint32_t k = 0;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      const std::uint32_t temp = rol(a, 5) + f + e + k + words[i];
      e = d;
      d = c;
      c = rol(b, 30);
      b = a;
      a = temp;
    }
    h0 += a;
    h1 += b;
    h2 += c;
    h3 += d;
    h4 += e;
  }

  const std::uint32_t hashes[5] = {h0, h1, h2, h3, h4};
  std::array<unsigned char, 20> digest{};
  for (int i = 0; i < 5; ++i) {
    digest[static_cast<std::size_t>(i) * 4] = static_cast<unsigned char>((hashes[i] >> 24) & 0xFF);
    digest[static_cast<std::size_t>(i) * 4 + 1] =
        static_cast<unsigned char>((hashes[i] >> 16) & 0xFF);
    digest[static_cast<std::size_t>(i) * 4 + 2] =
        static_cast<unsigned char>((hashes[i] >> 8) & 0xFF);
    digest[static_cast<std::size_t>(i) * 4 + 3] = static_cast<unsigned char>(hashes[i] & 0xFF);
  }
  return digest;
}

// base64 of a byte string
std::string base64(const unsigned char *data, std::size_t length) {
  static constexpr char kTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((length + 2) / 3) * 4);
  for (std::size_t i = 0; i < length; i += 3) {
    const std::uint32_t b0 = data[i];
    const std::uint32_t b1 = (i + 1 < length) ? data[i + 1] : 0;
    const std::uint32_t b2 = (i + 2 < length) ? data[i + 2] : 0;
    const std::uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
    out.push_back(kTable[(triple >> 18) & 63]);
    out.push_back(kTable[(triple >> 12) & 63]);
    out.push_back((i + 1 < length) ? kTable[(triple >> 6) & 63] : '=');
    out.push_back((i + 2 < length) ? kTable[triple & 63] : '=');
  }
  return out;
}

// ASCII lower-case copy
std::string lowerCopy(std::string_view text) {
  std::string out(text);
  for (char &ch : out) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}

// trim spaces and tabs
std::string_view trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.remove_suffix(1);
  }
  return text;
}

// true when value contains token, compared case-insensitively
bool headerContains(std::string_view value, std::string_view token) {
  return lowerCopy(value).find(lowerCopy(token)) != std::string::npos;
}

// buffered recv so the HTTP head and the first frame can share one socket
class SocketBuffer {
public:
  explicit SocketBuffer(SOCKET socket) : socket_(socket) {}

  // read through the header terminator; leaves any extra bytes pending
  bool readHeaders(std::string &out, std::size_t maxBytes) {
    while (pending_.find("\r\n\r\n") == std::string::npos) {
      if (pending_.size() >= maxBytes) {
        return false;
      }
      char temp[1024];
      const int got = ::recv(socket_, temp, static_cast<int>(sizeof(temp)), 0);
      if (got <= 0) {
        return false;
      }
      pending_.append(temp, static_cast<std::size_t>(got));
      if (pending_.size() > maxBytes) {
        return false;
      }
    }
    const std::size_t end = pending_.find("\r\n\r\n") + 4;
    out.assign(pending_, 0, end);
    pending_.erase(0, end);
    return true;
  }

  // one WebSocket frame from the browser (client frames are masked)
  FrameDecode readFrame() {
    if (!ensure(2)) {
      return {};
    }

    const auto lengthCode = static_cast<unsigned char>(pending_[1]) & 0x7F;
    const bool masked = (static_cast<unsigned char>(pending_[1]) & 0x80) != 0;
    std::size_t header = 2;
    if (lengthCode == 126) {
      header += 2;
    } else if (lengthCode == 127) {
      header += 8;
    }
    if (masked) {
      header += 4;
    }
    if (!ensure(header)) {
      return {};
    }

    std::uint64_t payloadLength = lengthCode;
    if (lengthCode == 126) {
      payloadLength = (static_cast<std::uint64_t>(static_cast<unsigned char>(pending_[2])) << 8) |
                      static_cast<unsigned char>(pending_[3]);
    } else if (lengthCode == 127) {
      payloadLength = 0;
      for (int i = 0; i < 8; ++i) {
        payloadLength = (payloadLength << 8) |
                        static_cast<unsigned char>(pending_[static_cast<std::size_t>(2 + i)]);
      }
    }
    if (payloadLength > static_cast<std::uint64_t>(Serializer::MAX_PAYLOAD_BYTES) + 4) {
      return {};
    }

    const std::size_t total = header + static_cast<std::size_t>(payloadLength);
    if (!ensure(total)) {
      return {};
    }

    FrameDecode decoded = decodeFrame(std::string_view(pending_.data(), total));
    if (decoded.status == FrameDecode::Status::Ok) {
      pending_.erase(0, decoded.consumed);
      if (!masked) {
        return {};
      }
    }
    return decoded;
  }

private:
  // pull from the socket until pending_ holds at least n bytes
  bool ensure(std::size_t count) {
    while (pending_.size() < count) {
      char temp[4096];
      const std::size_t need = count - pending_.size();
      const int ask = static_cast<int>(need < sizeof(temp) ? need : sizeof(temp));
      const int got = ::recv(socket_, temp, ask, 0);
      if (got <= 0) {
        return false;
      }
      pending_.append(temp, static_cast<std::size_t>(got));
    }
    return true;
  }

  SOCKET socket_;
  std::string pending_;
};

} // namespace

// Sec-WebSocket-Accept for a client key (RFC 6455)
std::string acceptValue(std::string_view clientKey) {
  std::string material(clientKey);
  material.append(kWebsocketGuid);
  const std::array<unsigned char, 20> digest = sha1(material);
  return base64(digest.data(), digest.size());
}

// FIN frame. maskKey is used only when masked is true.
std::string encodeFrame(WsOpcode opcode, std::string_view payload, bool masked,
                        std::uint32_t maskKey) {
  std::string out;
  out.push_back(static_cast<char>(0x80 | (static_cast<unsigned>(opcode) & 0x0F)));

  const std::size_t length = payload.size();
  const unsigned char maskBit = masked ? 0x80 : 0x00;
  if (length < 126) {
    out.push_back(static_cast<char>(maskBit | static_cast<unsigned char>(length)));
  } else if (length <= 65535) {
    out.push_back(static_cast<char>(maskBit | 126));
    out.push_back(static_cast<char>((length >> 8) & 0xFF));
    out.push_back(static_cast<char>(length & 0xFF));
  } else {
    out.push_back(static_cast<char>(maskBit | 127));
    const auto wide = static_cast<std::uint64_t>(length);
    for (int shift = 56; shift >= 0; shift -= 8) {
      out.push_back(static_cast<char>((wide >> shift) & 0xFF));
    }
  }

  if (!masked) {
    out.append(payload);
    return out;
  }

  const unsigned char mask[4] = {
      static_cast<unsigned char>((maskKey >> 24) & 0xFF),
      static_cast<unsigned char>((maskKey >> 16) & 0xFF),
      static_cast<unsigned char>((maskKey >> 8) & 0xFF),
      static_cast<unsigned char>(maskKey & 0xFF),
  };
  out.append(reinterpret_cast<const char *>(mask), 4);
  for (std::size_t i = 0; i < payload.size(); ++i) {
    out.push_back(static_cast<char>(static_cast<unsigned char>(payload[i]) ^ mask[i % 4]));
  }
  return out;
}

// Decode one frame. Incomplete means the buffer is shorter than the declared frame.
FrameDecode decodeFrame(std::string_view bytes) {
  FrameDecode result;
  if (bytes.size() < 2) {
    result.status = FrameDecode::Status::Incomplete;
    return result;
  }

  const auto first = static_cast<unsigned char>(bytes[0]);
  const auto second = static_cast<unsigned char>(bytes[1]);
  if ((first & 0x70) != 0 || (first & 0x80) == 0) {
    result.status = FrameDecode::Status::Error;
    return result;
  }

  const auto opcode = static_cast<WsOpcode>(first & 0x0F);
  const bool control = static_cast<unsigned>(opcode) >= 0x8;
  const bool known = opcode == WsOpcode::Text || opcode == WsOpcode::Binary ||
                     opcode == WsOpcode::Close || opcode == WsOpcode::Ping ||
                     opcode == WsOpcode::Pong;
  if (!known) {
    result.status = FrameDecode::Status::Error;
    return result;
  }

  const bool masked = (second & 0x80) != 0;
  const unsigned char lengthCode = second & 0x7F;
  std::size_t header = 2;
  if (lengthCode == 126) {
    header += 2;
  } else if (lengthCode == 127) {
    header += 8;
  }
  if (masked) {
    header += 4;
  }
  if (bytes.size() < header) {
    result.status = FrameDecode::Status::Incomplete;
    return result;
  }

  std::uint64_t payloadLength = lengthCode;
  if (lengthCode == 126) {
    payloadLength = (static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[2])) << 8) |
                    static_cast<unsigned char>(bytes[3]);
    if (payloadLength < 126) {
      result.status = FrameDecode::Status::Error;
      return result;
    }
  } else if (lengthCode == 127) {
    if ((static_cast<unsigned char>(bytes[2]) & 0x80) != 0) {
      result.status = FrameDecode::Status::Error;
      return result;
    }
    payloadLength = 0;
    for (int i = 0; i < 8; ++i) {
      payloadLength = (payloadLength << 8) |
                      static_cast<unsigned char>(bytes[static_cast<std::size_t>(2 + i)]);
    }
    if (payloadLength <= 65535) {
      result.status = FrameDecode::Status::Error;
      return result;
    }
  }

  if (control && payloadLength > 125) {
    result.status = FrameDecode::Status::Error;
    return result;
  }
  if (payloadLength > static_cast<std::uint64_t>(Serializer::MAX_PAYLOAD_BYTES) + 4) {
    result.status = FrameDecode::Status::Error;
    return result;
  }

  const std::size_t total = header + static_cast<std::size_t>(payloadLength);
  if (bytes.size() < total) {
    result.status = FrameDecode::Status::Incomplete;
    return result;
  }

  std::string payload(bytes.substr(header, static_cast<std::size_t>(payloadLength)));
  if (masked) {
    const std::size_t maskAt = header - 4;
    unsigned char mask[4];
    for (int i = 0; i < 4; ++i) {
      mask[i] = static_cast<unsigned char>(bytes[maskAt + static_cast<std::size_t>(i)]);
    }
    for (std::size_t i = 0; i < payload.size(); ++i) {
      payload[i] = static_cast<char>(static_cast<unsigned char>(payload[i]) ^ mask[i % 4]);
    }
  }

  result.status = FrameDecode::Status::Ok;
  result.consumed = total;
  result.opcode = opcode;
  result.payload = std::move(payload);
  return result;
}

// HTTP 101 response, or empty when the request is not a WebSocket upgrade
std::string upgradeResponse(std::string_view httpRequest) {
  const std::size_t end = httpRequest.find("\r\n\r\n");
  if (end == std::string_view::npos) {
    return {};
  }
  const std::string_view head = httpRequest.substr(0, end);
  const std::size_t lineEnd = head.find("\r\n");
  const std::string_view requestLine = lineEnd == std::string_view::npos ? head : head.substr(0, lineEnd);
  if (requestLine.size() < 4 || requestLine.substr(0, 4) != "GET ") {
    return {};
  }

  std::string key;
  std::string version;
  bool upgrade = false;
  bool connection = false;
  std::size_t cursor = lineEnd == std::string_view::npos ? head.size() : lineEnd + 2;
  while (cursor < head.size()) {
    const std::size_t next = head.find("\r\n", cursor);
    const std::string_view line =
        head.substr(cursor, next == std::string_view::npos ? std::string_view::npos : next - cursor);
    const std::size_t colon = line.find(':');
    if (colon != std::string_view::npos) {
      const std::string name = lowerCopy(trim(line.substr(0, colon)));
      const std::string_view value = trim(line.substr(colon + 1));
      if (name == "upgrade") {
        upgrade = headerContains(value, "websocket");
      } else if (name == "connection") {
        connection = headerContains(value, "upgrade");
      } else if (name == "sec-websocket-key") {
        key = std::string(value);
      } else if (name == "sec-websocket-version") {
        version = std::string(value);
      }
    }
    if (next == std::string_view::npos) {
      break;
    }
    cursor = next + 2;
  }

  if (!upgrade || !connection || key.empty() || version != "13") {
    return {};
  }

  return "HTTP/1.1 101 Switching Protocols\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Accept: " +
         acceptValue(key) + "\r\n\r\n";
}

WebConnection::WebConnection(ConnectionManager &connections) : connections_(connections) {}

WebConnection::~WebConnection() {
  stopListening();
  joinClientThreads();
}

// bind the browser port. Server runs acceptLoop on its own thread.
bool WebConnection::startListening(std::uint16_t port) {
  if (listening_.load()) {
    Logger::logWarning("WebConnection", "Already listening");
    return false;
  }

  listeningSocket_ = socket_io::listenTo(port);
  if (listeningSocket_ == INVALID_SOCKET) {
    Logger::logError("WebConnection", "Failed to listen on port " + std::to_string(port));
    return false;
  }

  sockaddr_in bound{};
  int boundLength = sizeof(bound);
  if (getsockname(listeningSocket_, reinterpret_cast<sockaddr *>(&bound), &boundLength) != 0) {
    socket_io::close(listeningSocket_);
    listeningSocket_ = INVALID_SOCKET;
    return false;
  }
  listenPort_ = ntohs(bound.sin_port);
  listening_ = true;
  Logger::logInfo("WebConnection", "Listening on port " + std::to_string(listenPort_));
  return true;
}

// close the listen socket and wake browser reads. Does not join handler threads.
void WebConnection::stopListening() {
  std::vector<SOCKET> sockets;
  {
    std::lock_guard<std::mutex> lock(socketsMutex_);
    if (!listening_.exchange(false)) {
      return;
    }
    if (listeningSocket_ != INVALID_SOCKET) {
      socket_io::close(listeningSocket_);
      listeningSocket_ = INVALID_SOCKET;
    }
    sockets = liveSockets_;
  }
  for (SOCKET socket : sockets) {
    ::shutdown(socket, SD_BOTH);
  }
}

void WebConnection::joinClientThreads() {
  std::vector<std::thread> clients;
  {
    std::lock_guard<std::mutex> lock(threadsMutex_);
    clients.swap(clientThreads_);
  }
  for (std::thread &thread : clients) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

bool WebConnection::isListening() const { return listening_.load(); }

std::uint16_t WebConnection::port() const { return listenPort_; }

bool WebConnection::track(SOCKET socket) {
  std::lock_guard<std::mutex> lock(socketsMutex_);
  if (!listening_.load()) {
    return false;
  }
  liveSockets_.push_back(socket);
  return true;
}

void WebConnection::untrack(SOCKET socket) {
  std::lock_guard<std::mutex> lock(socketsMutex_);
  for (auto it = liveSockets_.begin(); it != liveSockets_.end(); ++it) {
    if (*it == socket) {
      liveSockets_.erase(it);
      break;
    }
  }
}

void WebConnection::acceptLoop() {
  while (listening_.load()) {
    const SOCKET browser = socket_io::acceptFrom(listeningSocket_);
    if (browser == INVALID_SOCKET) {
      break;
    }
    if (!track(browser)) {
      socket_io::close(browser);
      continue;
    }

    std::lock_guard<std::mutex> lock(threadsMutex_);
    if (!listening_.load()) {
      untrack(browser);
      socket_io::close(browser);
      break;
    }
    clientThreads_.emplace_back([this, browser] { handleBrowser(browser); });
  }
}

void WebConnection::handleBrowser(SOCKET browser) {
  bool handedOff = false;
  const auto finish = [&] {
    if (!handedOff) {
      socket_io::close(browser);
    }
    untrack(browser);
  };

  SocketBuffer buffer(browser);
  std::string request;
  if (!buffer.readHeaders(request, kMaxHttpHeaderBytes)) {
    finish();
    return;
  }
  const std::string response = upgradeResponse(request);
  if (response.empty() ||
      !socket_io::sendExact(browser, response.data(), static_cast<int>(response.size()))) {
    finish();
    return;
  }

  connections_.markWebSocket(browser);
  if (!connections_.openSession(browser)) {
    handedOff = true;
    finish();
    return;
  }
  handedOff = true;

  while (listening_.load() && connections_.isListening()) {
    const FrameDecode frame = buffer.readFrame();
    if (frame.status != FrameDecode::Status::Ok) {
      break;
    }
    if (frame.opcode == WsOpcode::Ping) {
      const std::string pong = encodeFrame(WsOpcode::Pong, frame.payload, false);
      const auto sessions = connections_.getSessions();
      const auto it = sessions.find(browser);
      if (it == sessions.end()) {
        break;
      }
      std::lock_guard<std::mutex> lock(it->second->sendMutex());
      if (!socket_io::sendExact(browser, pong.data(), static_cast<int>(pong.size()))) {
        break;
      }
      continue;
    }
    if (frame.opcode == WsOpcode::Pong) {
      continue;
    }
    if (frame.opcode == WsOpcode::Close || frame.opcode != WsOpcode::Binary) {
      break;
    }

    const std::optional<Packet> packet = Serializer::deserialize(frame.payload);
    if (!packet || !connections_.dispatchPacket(browser, *packet)) {
      break;
    }
  }

  connections_.releaseClient(browser);
  finish();
}

} // namespace web_connection
