/**
 * Endpoint ring
 *
 * @brief Ordered host:port list with a wrapping cursor.
 * @date 27-09-2026
 */

#pragma once
#include "config/config.h"
#include "utils/models/client_endpoint.h"
#include <cstddef>
#include <mutex>
#include <random>
#include <vector>

// directory endpoints reported as up, then seed entries not already listed.
// a usable current endpoint stays first so the next hop tries the directory.
std::vector<ServerPoint> mergeFailoverList(const ServerPoint &current,
                                           const std::vector<ServerPoint> &seed,
                                           const std::vector<ClientEndpoint> &directory);

// ceiling for attempt n: min(8000 ms, 200 ms * 2^n)
int backoffCeilingMs(int attempt);

// full jitter: uniform in [0, ceiling]. --test sleeps 0 ms so unit tests stay fast.
int fullJitterDelayMs(int attempt, std::mt19937 &rng);

class EndpointRing {
public:
  // replace the list; keep the cursor on the same host:port when it is still present
  void replace(std::vector<ServerPoint> next);

  // endpoint the cursor points at (port 0 when the list is empty)
  ServerPoint current() const;

  // step to the next entry and wrap
  void advance();

  // number of endpoints
  std::size_t size() const;

private:
  std::vector<ServerPoint> points; // failover order
  std::size_t index{0};            // cursor
  mutable std::mutex mutex;        // guards points and index
};
