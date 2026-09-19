/**
 * Main gui page — GUI entry point
 *
 * @date 15-09-2026
 */

#include "server/gui/main_page.h"
#include "server/gui/main_window.h"
#include <QApplication>

// run the main page
int MainPage::run(int argc, char *argv[], Server &server) {
  QApplication app(argc, argv);
  MainWindow window(nullptr, &server);
  window.show();
  return app.exec();
}
