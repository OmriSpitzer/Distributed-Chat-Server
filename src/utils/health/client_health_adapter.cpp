/**
 * Client health adapter
 *
 * @brief Adapts Client to IHealthCheck
 * @date 23-09-2026
 *
 * Design pattern: Adapter
 * Health check adapter for the client
 */

#include "utils/health/client_health_adapter.h"
#include "client/client.h"

// constructor
ClientHealthAdapter::ClientHealthAdapter(Client &client) : client_(client) {}

// check the health
HealthReport ClientHealthAdapter::check() {
  // check if the client is connected
  if (client_.isAlive()) {
    return {"client", HealthStatus::Up, "connected"};
  } else {
    return {"client", HealthStatus::Down, "not connected"};
  }
}
