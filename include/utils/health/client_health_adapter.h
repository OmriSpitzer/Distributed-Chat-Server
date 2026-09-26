/**
 * Client health adapter header
 *
 * @date 23-09-2026
 */
#pragma once
#include "utils/health/i_health_check.h"

class Client;

class ClientHealthAdapter : public IHealthCheck {
public:
  // constructor
  explicit ClientHealthAdapter(Client &client);

  // check the health
  HealthReport check() override;

private:
  Client &client_; // client
};
