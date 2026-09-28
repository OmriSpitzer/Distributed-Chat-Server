/**
 * Endpoint ring and reconnect backoff tests
 *
 * @date 27-09-2026
 */

#include "client/endpoint_ring.h"
#include "config/config.h"
#include <catch2/catch_test_macros.hpp>
#include <random>

/**
 * 1. round-robin wraps
 * 2. replace keeps the current endpoint
 * 3. replace of a missing endpoint restarts at the front
 * 4. backoff ceiling doubles until 8000 ms
 * 5. full jitter stays inside the ceiling; --test sleeps 0
 * 6. directory-up endpoints come before seed-only entries
 * 7. the connected endpoint stays first; the next hop is directory-up
 * 8. empty directory keeps the seed; blank seed entries are dropped
 * 9. directory entries that are not up are ignored; duplicates collapse
 */

// 1. round-robin wraps
TEST_CASE("EndpointRing walks the list and wraps", "[endpoint_ring][failover]") {
  EndpointRing ring;
  ring.replace({
      ServerPoint{"127.0.0.1", 1},
      ServerPoint{"127.0.0.1", 2},
      ServerPoint{"10.0.0.1", 3},
  });

  REQUIRE(ring.size() == 3);
  REQUIRE(ring.current().port == 1);
  ring.advance();
  REQUIRE(ring.current().port == 2);
  ring.advance();
  REQUIRE(ring.current().port == 3);
  ring.advance();
  REQUIRE(ring.current().host == "127.0.0.1");
  REQUIRE(ring.current().port == 1);
}

// 2. replace keeps the current endpoint
TEST_CASE("EndpointRing replace keeps the cursor host", "[endpoint_ring][failover]") {
  EndpointRing ring;
  ring.replace({
      ServerPoint{"127.0.0.1", 1},
      ServerPoint{"127.0.0.1", 2},
      ServerPoint{"10.0.0.1", 3},
  });
  ring.advance();
  REQUIRE(ring.current().port == 2);

  ring.replace({
      ServerPoint{"10.0.0.1", 3},
      ServerPoint{"127.0.0.1", 2},
  });
  REQUIRE(ring.current().port == 2);
  ring.advance();
  REQUIRE(ring.current().port == 3);
}

// 3. replace of a missing endpoint restarts at the front
TEST_CASE("EndpointRing replace drops a missing cursor", "[endpoint_ring][failover][edge]") {
  EndpointRing ring;
  ring.replace({ServerPoint{"127.0.0.1", 9}});
  ring.replace({ServerPoint{"10.0.0.2", 4}, ServerPoint{"10.0.0.3", 5}});
  REQUIRE(ring.current().port == 4);

  ring.replace({});
  REQUIRE(ring.size() == 0);
  REQUIRE(ring.current().port == 0);
  ring.advance();
  REQUIRE(ring.current().port == 0);
}

// 4. backoff ceiling doubles until 8000 ms
TEST_CASE("Backoff ceiling doubles and caps", "[failover][backoff]") {
  REQUIRE(backoffCeilingMs(0) == 200);
  REQUIRE(backoffCeilingMs(1) == 400);
  REQUIRE(backoffCeilingMs(2) == 800);
  REQUIRE(backoffCeilingMs(3) == 1600);
  REQUIRE(backoffCeilingMs(4) == 3200);
  REQUIRE(backoffCeilingMs(5) == 6400);
  REQUIRE(backoffCeilingMs(6) == 8000);
  REQUIRE(backoffCeilingMs(30) == 8000);
  REQUIRE(backoffCeilingMs(-1) == 200);
}

// 5. full jitter stays inside the ceiling; --test sleeps 0
TEST_CASE("Full jitter stays within the ceiling", "[failover][backoff][jitter]") {
  const bool previous = config::TEST_MODE;
  config::TEST_MODE = false;

  std::mt19937 rng{1};
  for (int attempt = 0; attempt < 8; ++attempt) {
    const int ceiling = backoffCeilingMs(attempt);
    for (int sample = 0; sample < 40; ++sample) {
      const int delay = fullJitterDelayMs(attempt, rng);
      REQUIRE(delay >= 0);
      REQUIRE(delay <= ceiling);
    }
  }

  config::TEST_MODE = true;
  REQUIRE(fullJitterDelayMs(4, rng) == 0);
  config::TEST_MODE = previous;
}

// 6. directory-up endpoints come before seed-only entries
TEST_CASE("Failover list prefers directory endpoints", "[endpoint_ring][failover]") {
  const std::vector<ServerPoint> seed = {
      ServerPoint{"127.0.0.1", 1},
      ServerPoint{"127.0.0.1", 2},
  };
  const std::vector<ClientEndpoint> directory = {
      ClientEndpoint("node-c", "10.0.0.3", 3),
      ClientEndpoint("node-b", "127.0.0.1", 2),
  };

  const auto merged = mergeFailoverList(ServerPoint{}, seed, directory);
  REQUIRE(merged.size() == 3);
  REQUIRE(merged[0].host == "10.0.0.3");
  REQUIRE(merged[0].port == 3);
  REQUIRE(merged[1].port == 2);
  REQUIRE(merged[2].port == 1);
}

// 7. the connected endpoint stays first; the next hop is directory-up
TEST_CASE("Failover list keeps the connected endpoint first", "[endpoint_ring][failover]") {
  const std::vector<ServerPoint> seed = {
      ServerPoint{"127.0.0.1", 1},
      ServerPoint{"127.0.0.1", 2},
  };
  const std::vector<ClientEndpoint> directory = {
      ClientEndpoint("node-c", "10.0.0.3", 3),
      ClientEndpoint("node-d", "10.0.0.4", 4),
  };

  EndpointRing ring;
  ring.replace(mergeFailoverList(ServerPoint{"127.0.0.1", 1}, seed, directory));
  REQUIRE(ring.size() == 4);
  REQUIRE(ring.current().port == 1);
  ring.advance();
  REQUIRE(ring.current().port == 3);
  ring.advance();
  REQUIRE(ring.current().port == 4);
  ring.advance();
  REQUIRE(ring.current().port == 2);
}

// 8. empty directory keeps the seed; blank seed entries are dropped
TEST_CASE("Failover list keeps the seed when the directory is empty",
          "[endpoint_ring][failover][edge]") {
  const std::vector<ServerPoint> seed = {
      ServerPoint{"127.0.0.1", 1},
      ServerPoint{"", 2},
      ServerPoint{"127.0.0.1", 0},
      ServerPoint{"127.0.0.1", 1},
      ServerPoint{"10.0.0.2", 5},
  };

  const auto merged = mergeFailoverList(ServerPoint{}, seed, {});
  REQUIRE(merged.size() == 2);
  REQUIRE(merged[0].port == 1);
  REQUIRE(merged[1].host == "10.0.0.2");
  REQUIRE(merged[1].port == 5);
}

// 9. directory entries that are not up are ignored; duplicates collapse
TEST_CASE("Failover list ignores directory entries that are not up",
          "[endpoint_ring][failover][edge]") {
  const std::vector<ServerPoint> seed = {ServerPoint{"127.0.0.1", 1}};
  const std::vector<ClientEndpoint> directory = {
      ClientEndpoint("", "10.0.0.9", 9),
      ClientEndpoint("node-a", "", 7),
      ClientEndpoint("node-a", "10.0.0.8", 0),
      ClientEndpoint("node-a", "10.0.0.8", 8),
      ClientEndpoint("node-a", "10.0.0.8", 8),
  };

  const auto merged = mergeFailoverList(ServerPoint{}, seed, directory);
  REQUIRE(merged.size() == 2);
  REQUIRE(merged[0].host == "10.0.0.8");
  REQUIRE(merged[0].port == 8);
  REQUIRE(merged[1].port == 1);
}
