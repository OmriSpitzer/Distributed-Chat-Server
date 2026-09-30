/**
 * Database health adapter
 *
 * @brief Adapts DatabaseManager to IHealthCheck
 * @date 23-09-2026
 *
 * Design pattern: Adapter
 * Health check adapter for the database manager
 */

#include "utils/health/db_health_adapter.h"
#include "server/database_manager.h"

// constructor
DbHealthAdapter::DbHealthAdapter(DatabaseManager &db) : db_(db) {}

// check the health
HealthReport DbHealthAdapter::check() {
  // check if the database is reachable
  if (db_.ping()) {
    return {"database", HealthStatus::Up, "ok"};
  } else {
    return {"database", HealthStatus::Down, "ping failed"};
  }
}
