/**
 * Client GUI entry point
 *
 * @date 16-09-2026
 */

#include "client/gui/pages/main_page.h"
#include "client/gui/pages/dashboard_page.h"
#include <QApplication>
#include <QMainWindow>

int MainPage::run(int argc, char *argv[], Client &client) {
  // create the application
  QApplication app(argc, argv);

  // create the main window
  QMainWindow window;
  window.setWindowTitle("Distributed Chat");
  window.resize(1200, 760);
  window.setCentralWidget(new DashboardPage(&window, &client));

  // show the window
  window.show();
  return app.exec();
}
