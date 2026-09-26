/**
 * Heartbeat health adapter
 *
 * @brief Adapts Heartbeat to IHealthCheck
 * @date 23-09-2026
 *
 * Design pattern: Adapter
 * Health check adapter for the heartbeat
 */

#include "utils/health/heartbeat_health_adapter.h"
#include "server/heartbeat.h"

// constructor
HeartbeatHealthAdapter::HeartbeatHealthAdapter(Heartbeat &heartbeat) : heartbeat_(heartbeat) {}

// check the health
HealthReport HeartbeatHealthAdapter::check() {
  // check if the heartbeat is running
  if (heartbeat_.isRunning()) {
    return {"heartbeat", HealthStatus::Up, "running"};
  } else {
    return {"heartbeat", HealthStatus::Down, "not running"};
  }
}
