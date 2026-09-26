/**
 * ClientEndpoint unit tests
 *
 * @date 25-09-2026
 */

#include "utils/models/client_endpoint.h"
#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

TEST_CASE("ClientEndpoint serialize round-trip", "[client_endpoint]") {
  const ClientEndpoint endpoint("node-a", "127.0.0.1", 5555, INVALID_SOCKET);
  REQUIRE(endpoint.serialize() == "endpoint(node-a|127.0.0.1|5555)");

  const ClientEndpoint back = ClientEndpoint::deserialize(endpoint.serialize());
  REQUIRE(back.nodeId == "node-a");
  REQUIRE(back.host == "127.0.0.1");
  REQUIRE(back.port == 5555);
  REQUIRE(back.getSocket() == INVALID_SOCKET);
  REQUIRE(back.isLiveClientEndpoint());
}

TEST_CASE("ClientEndpoint deserialize rejects bad input", "[client_endpoint][edge]") {
  REQUIRE_THROWS_AS(ClientEndpoint::deserialize("endpoint(node|host)"), std::invalid_argument);
  REQUIRE_THROWS_AS(ClientEndpoint::deserialize("endpoint(node|host|0)"), std::invalid_argument);
  REQUIRE_THROWS_AS(ClientEndpoint::deserialize("endpoint(|host|1)"), std::invalid_argument);
  REQUIRE_THROWS_AS(ClientEndpoint::deserialize("bad"), std::invalid_argument);
}

TEST_CASE("ClientEndpoint isLiveClientEndpoint", "[client_endpoint]") {
  REQUIRE_FALSE(ClientEndpoint("", "127.0.0.1", 1).isLiveClientEndpoint());
  REQUIRE_FALSE(ClientEndpoint("n", "", 1).isLiveClientEndpoint());
  REQUIRE_FALSE(ClientEndpoint("n", "127.0.0.1", 0).isLiveClientEndpoint());
  REQUIRE(ClientEndpoint("n", "127.0.0.1", 1).isLiveClientEndpoint());
}
