/**
 * Health Monitor class header file
 *
 * @date 23-09-2026
 */
#pragma once
#include "utils/health/i_health_check.h"
#include <memory>
#include <vector>

class HealthMonitor : public IHealthCheck {
public:
  // add a health check
  void add(std::unique_ptr<IHealthCheck> check);

  // check all the health checks
  std::vector<HealthReport> checkAll() const;

  // check the overall health
  HealthReport check() override;

private:
  std::vector<std::unique_ptr<IHealthCheck>> healthChecks; // health checks
};
