/**
 * Client entry point
 *
 * @date 06-09-2026
 */

#include "client/client.h"
#include "config/config.h"

int main(int argc, char *argv[]) {
  if (!config::parseArgs(argc, argv)) {
    return 1;
  }

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
