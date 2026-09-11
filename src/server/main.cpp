/**
 * Server entry point
 *
 * @brief Server entry point. Parses command line arguments and starts the server.
 * @date 11-09-2026
 */

#include "config/config.h"
#include "server/server.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <windows.h>

// global variables for console control signals
static std::atomic<bool> g_stop{false};
static std::mutex g_mu;
static std::condition_variable g_cv;

// handle console control signals
BOOL WINAPI onConsoleCtrl(DWORD) {
  g_stop = true;
  g_cv.notify_all();
  return TRUE;
}

// main function
int main(int argc, char *argv[]) {
  // parse the command line arguments
  if (!config::parseArgs(argc, argv)) {
    return 1;
  }

  // create the server
  Server server;

  // start the server and show the dashboard
  server.start();
  server.dashboard();

  // set the console control handler
  SetConsoleCtrlHandler(onConsoleCtrl, TRUE);

  // wait for the server to stop
  {
    std::unique_lock lock(g_mu);
    g_cv.wait(lock, [] { return g_stop.load(); });
  }
  server.stop();
  return 0;
}
