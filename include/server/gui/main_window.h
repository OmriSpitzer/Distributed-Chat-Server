/**
 * Server GUI main window shell
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/server.h"
#include <QMainWindow>


class LogPanel;
class PortsPanel;
class RoomsPanel;
class UsersPanel;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr, Server *server = nullptr);

private:
  static const int DEFAULT_WIDTH = 1100; // default width of the window
  static const int DEFAULT_HEIGHT = 700; // default height of the window
  Server *server{nullptr};               // pointer to the server instance

  PortsPanel *ports{nullptr};
  UsersPanel *users{nullptr};
  RoomsPanel *rooms{nullptr};
  LogPanel *log{nullptr};

  // display header
  QWidget *buildHeader(QWidget *parent);
};
