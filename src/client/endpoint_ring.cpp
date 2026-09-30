/**
 * Endpoint ring
 *
 * @brief Round-robin cursor over client failover endpoints.
 * @date 27-09-2026
 */

#include "client/endpoint_ring.h"

namespace {

bool usable(const ServerPoint &point) { return !point.host.empty() && point.port != 0; }

void appendUnique(std::vector<ServerPoint> &out, const ServerPoint &point) {
  if (!usable(point)) {
    return;
  }
  for (const auto &have : out) {
    if (have.host == point.host && have.port == point.port) {
      return;
    }
  }
  out.push_back(point);
}

} // namespace

// directory endpoints reported as up, then seed entries not already listed.
// a usable current endpoint stays first so the next hop tries the directory.
std::vector<ServerPoint> mergeFailoverList(const ServerPoint &current,
                                           const std::vector<ServerPoint> &seed,
                                           const std::vector<ClientEndpoint> &directory) {
  std::vector<ServerPoint> merged;
  appendUnique(merged, current);
  for (const auto &endpoint : directory) {
    if (endpoint.isLiveClientEndpoint()) {
      appendUnique(merged, ServerPoint{endpoint.host, endpoint.port});
    }
  }
  for (const auto &point : seed) {
    appendUnique(merged, point);
  }
  return merged;
}

// replace the list; keep the cursor on the same host:port when it is still present
void EndpointRing::replace(std::vector<ServerPoint> next) {
  std::lock_guard<std::mutex> lock(mutex);
  ServerPoint previous{};
  const bool had = !points.empty();
  if (had) {
    previous = points[index];
  }

  points = std::move(next);
  index = 0;
  if (!had || points.empty()) {
    return;
  }

  for (std::size_t i = 0; i < points.size(); ++i) {
    if (points[i].host == previous.host && points[i].port == previous.port) {
      index = i;
      return;
    }
  }
}

// endpoint the cursor points at (port 0 when the list is empty)
ServerPoint EndpointRing::current() const {
  std::lock_guard<std::mutex> lock(mutex);
  if (points.empty()) {
    return {};
  }
  return points[index];
}

// step to the next entry and wrap
void EndpointRing::advance() {
  std::lock_guard<std::mutex> lock(mutex);
  if (points.empty()) {
    return;
  }
  index = (index + 1) % points.size();
}

// number of endpoints
std::size_t EndpointRing::size() const {
  std::lock_guard<std::mutex> lock(mutex);
  return points.size();
}

// ceiling for attempt n: min(8000 ms, 200 ms * 2^n)
int backoffCeilingMs(int attempt) {
  constexpr int kBaseMs = 200;
  constexpr int kCapMs = 8000;
  int ceiling = kBaseMs;
  const int steps = attempt < 0 ? 0 : attempt;
  for (int i = 0; i < steps; ++i) {
    if (ceiling >= kCapMs / 2) {
      ceiling = kCapMs;
      break;
    }
    ceiling *= 2;
  }
  return ceiling;
}

// full jitter: uniform in [0, ceiling]. --test sleeps 0 ms so unit tests stay fast.
int fullJitterDelayMs(int attempt, std::mt19937 &rng) {
  if (config::TEST_MODE) {
    return 0;
  }
  const int ceiling = backoffCeilingMs(attempt);
  std::uniform_int_distribution<int> dist(0, ceiling);
  return dist(rng);
}
