/**
 * Server health adapter header
 *
 * @date 23-09-2026
 */
#pragma once
#include "utils/health/i_health_check.h"

class Server;

class ServerHealthAdapter : public IHealthCheck {
public:
  // constructor
  explicit ServerHealthAdapter(Server &server);

  // check the health
  HealthReport check() override;

private:
  Server &server_; // server
};
