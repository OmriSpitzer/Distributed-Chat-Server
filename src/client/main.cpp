/**
 * Client entry point
 *
 * @date 13-09-2026
 */

#include "client/client.h"
#include "config/config.h"
#include <atomic>
#include <windows.h>

// global variables for console control signals
static std::atomic<bool> g_stop{false};

// handle console control signals
BOOL WINAPI onConsoleCtrl(DWORD) {
  g_stop = true;
  return TRUE;
}

int main(int argc, char *argv[]) {
  if (!config::parseArgs(argc, argv)) {
    return 1;
  }

  // set the console control handler
  SetConsoleCtrlHandler(onConsoleCtrl, TRUE);

  // start the client
  Client client;
  if (!client.start()) {
    return 1;
  }

  if (config::TEST_MODE) {
    // console mode (original)
    while (client.isAlive() && !g_stop.load()) {
      client.showDashboard();
    }
    client.stop();

    return 0;
  } else {
    // gui mode (Page stub)
    const int rc = client.showDashboard_2(argc, argv);
    client.stop();
    return rc;
  }
}