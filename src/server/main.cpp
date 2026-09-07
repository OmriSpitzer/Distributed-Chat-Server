/**
 * Server entry point
 *
 * @date 14-07-2026
 */

#include "config/config.h"
#include "server/server.h"

int main(int argc, char *argv[]) {
  if (!config::parseArgs(argc, argv)) {
    return 1;
  }

  Server server;

  // Starting server
  server.start();
  server.dashboard();

  while (server.isAlive()) {
  }
  // Stopping server
  server.stop();
  return 0;
}
