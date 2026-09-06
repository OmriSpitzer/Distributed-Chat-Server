/**
 * Client entry point
 *
 * @date 06-09-2026
 */

#include "client/client.h"

int main() {
  Client client;
  // start the client
  if (!client.start()) {
    return 1;
  }

  // show the dashboard
  while (client.isAlive()) {
    client.showDashboard();
  }

  return 0;
}
