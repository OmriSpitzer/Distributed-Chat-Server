/**
 * Database health adapter header
 *
 * @date 23-09-2026
 */
#pragma once
#include "utils/health/i_health_check.h"

class DatabaseManager;

class DbHealthAdapter : public IHealthCheck {
public:
  // constructor
  explicit DbHealthAdapter(DatabaseManager &db);

  // check the health
  HealthReport check() override;

private:
  DatabaseManager &db_; // database manager
};
