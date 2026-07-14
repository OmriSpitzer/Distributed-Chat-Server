/**
 * Server entry point
 *
 * @date 14-07-2026
 */

#include "server/server.h"

int main() {
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
