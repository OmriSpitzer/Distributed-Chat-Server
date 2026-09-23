/**
 * Health Check interface header file
 *
 * @date 23-09-2026
 */
#pragma once
#include <string>

// health status enum
enum class HealthStatus { Up, Degraded, Down };

// status to string (for a bare HealthStatus)
inline std::string healthStatusToString(HealthStatus status) {
  switch (status) {
  case HealthStatus::Up:
    return "Up";
  case HealthStatus::Degraded:
    return "Degraded";
  case HealthStatus::Down:
    return "Down";
  }
  return "Unknown";
}

// health report struct
struct HealthReport {
  std::string name;                        // name of the health check
  HealthStatus status{HealthStatus::Down}; // health status
  std::string detail;                      // detail of the health check

  // status to string
  std::string statusToString() const { return healthStatusToString(status); }
};

class IHealthCheck {
public:
  // destructor
  virtual ~IHealthCheck() = default;

  // check the health
  virtual HealthReport check() = 0;
};
