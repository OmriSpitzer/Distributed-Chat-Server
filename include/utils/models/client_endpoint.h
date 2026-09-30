/**
 * ClientEndpoint class header file
 *
 * @date 25-09-2026
 */

#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <winsock2.h>
#ifdef ERROR
#undef ERROR
#endif

class ClientEndpoint {
public:
  // constructor; socket is the local gossip fd and is not part of the wire form
  ClientEndpoint(std::string_view nodeId, std::string_view host, std::uint16_t port,
                 SOCKET socket = INVALID_SOCKET);

  // getters
  SOCKET getSocket() const;

  // setters
  void setSocket(SOCKET socket);

  // true when host/port form a usable client listen address
  bool isLiveClientEndpoint() const;

  // serialize
  std::string serialize() const;

  // deserialize
  static ClientEndpoint deserialize(const std::string &serialized);

  std::string nodeId; // NODE_ID of the peer
  std::string host;   // client host address
  std::uint16_t port; // client port number

private:
  SOCKET socket; // gossip peer fd
};
