/**
 * Server health adapter
 *
 * @brief Adapts Server to IHealthCheck
 * @date 23-09-2026
 *
 * Design pattern: Adapter
 * Health check adapter for the server
 */

#include "utils/health/server_health_adapter.h"
#include "server/server.h"

// constructor
ServerHealthAdapter::ServerHealthAdapter(Server &server) : server_(server) {}

// check the health
HealthReport ServerHealthAdapter::check() {
  // check if the server is alive
  if (server_.isAlive()) {
    return {"server", HealthStatus::Up, "alive"};
  } else {
    return {"server", HealthStatus::Down, "not alive"};
  }
}
