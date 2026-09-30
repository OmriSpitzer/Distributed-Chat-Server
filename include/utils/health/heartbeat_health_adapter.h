/**
 * Heartbeat health adapter header
 *
 * @date 23-09-2026
 */
#pragma once
#include "utils/health/i_health_check.h"

class Heartbeat;

class HeartbeatHealthAdapter : public IHealthCheck {
public:
  // constructor
  explicit HeartbeatHealthAdapter(Heartbeat &heartbeat);

  // check the health
  HealthReport check() override;

private:
  Heartbeat &heartbeat_; // heartbeat
};
