/**
 * ClientEndpoint class implementation file
 *
 * @brief Client listen endpoint advertised by a gossip peer.
 * @date 25-09-2026
 *
 * ClientEndpoint class with fields: nodeId, host, port, socket
 * Serialized format: endpoint(nodeId|host|port)
 * The socket is a local handle and is not serialized.
 */

#include "utils/models/client_endpoint.h"
#include <stdexcept>

// constructor
ClientEndpoint::ClientEndpoint(std::string_view nodeId, std::string_view host, std::uint16_t port,
                               SOCKET socket)
    : nodeId(nodeId), host(host), port(port), socket(socket) {}

// getters
SOCKET ClientEndpoint::getSocket() const { return this->socket; }

// setters
void ClientEndpoint::setSocket(SOCKET socket) { this->socket = socket; }

// true when host/port form a usable client listen address
bool ClientEndpoint::isLiveClientEndpoint() const {
  return !this->nodeId.empty() && !this->host.empty() && this->port != 0;
}

// serialize
std::string ClientEndpoint::serialize() const {
  return "endpoint(" + this->nodeId + "|" + this->host + "|" + std::to_string(this->port) + ")";
}

// deserialize
ClientEndpoint ClientEndpoint::deserialize(const std::string &serialized) {
  static const std::string prefix = "endpoint(";

  // check correct serialization format
  if (serialized.size() < prefix.size() + 1 || serialized.compare(0, prefix.size(), prefix) != 0 ||
      serialized.back() != ')') {
    throw std::invalid_argument("Invalid serialized client endpoint");
  }

  // extract the body of the serialized endpoint
  const std::string body = serialized.substr(prefix.size(), serialized.size() - prefix.size() - 1);
  const std::size_t first = body.find('|');
  const std::size_t second =
      (first == std::string::npos) ? std::string::npos : body.find('|', first + 1);

  // check if all the fields are present
  if (first == std::string::npos || second == std::string::npos ||
      body.find('|', second + 1) != std::string::npos) {
    throw std::invalid_argument("Invalid serialized client endpoint");
  }

  // extract the fields from the body
  const std::string nodeId = body.substr(0, first);
  const std::string host = body.substr(first + 1, second - first - 1);
  const std::string portText = body.substr(second + 1);
  if (nodeId.empty() || host.empty()) {
    throw std::invalid_argument("Invalid serialized client endpoint");
  }

  // convert the port string to an integer
  unsigned long port = 0;
  try {
    port = std::stoul(portText);
  } catch (const std::exception &) {
    throw std::invalid_argument("Invalid serialized client endpoint port");
  }
  if (port == 0 || port > 65535) {
    throw std::invalid_argument("Invalid serialized client endpoint port");
  }

  return ClientEndpoint(nodeId, host, static_cast<std::uint16_t>(port));
}
