/**
 * Health Monitor class implementation file
 *
 * @brief Monitors the health of the system
 * @date 23-09-2026
 *
 * Design pattern: Composite
 * Aggregates leaf IHealthCheck adapters into one overall report.
 */

#include "utils/health/health_monitor.h"
#include "utils/health/i_health_check.h"
#include <memory>

// add a health check
void HealthMonitor::add(std::unique_ptr<IHealthCheck> check) {
  healthChecks.push_back(std::move(check));
}

// check all the health checks
std::vector<HealthReport> HealthMonitor::checkAll() const {
  std::vector<HealthReport> out;
  out.reserve(healthChecks.size());
  for (const auto &c : healthChecks) {
    out.push_back(c->check());
  }
  return out;
}

// check the overall health
HealthReport HealthMonitor::check() {
  HealthReport overall{"cluster", HealthStatus::Up, {}};
  for (const auto &r : checkAll()) {
    if (r.status == HealthStatus::Down) {
      overall.status = HealthStatus::Down;
      if (!overall.detail.empty()) {
        overall.detail += "; ";
      }
      overall.detail += r.name + ":Down";
    } else if (r.status == HealthStatus::Degraded && overall.status != HealthStatus::Down) {
      overall.status = HealthStatus::Degraded;
    }
  }
  return overall;
}
