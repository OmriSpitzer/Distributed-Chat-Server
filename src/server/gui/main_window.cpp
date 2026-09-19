/**
 * Server GUI main window implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/main_window.h"
#include "server/gui/panels/log_panel.h"
#include "server/gui/panels/ports_panel.h"
#include "server/gui/panels/rooms_panel.h"
#include "server/gui/panels/users_panel.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

namespace {
const char *STYLE_SHEET = R"(
    /* main window */
    QMainWindow { background:rgb(229, 229, 230); }

    /* panels */
    QWidget#Panel {
      background: #ffffff;
      border: 3px solidrgb(237, 238, 240);
      border-radius: 10px;
    }

    /* labels */
    QLabel#HeaderTitle {
      font-size: 22px;
      font-weight: 700;
      color: #1a2332;
    }

    /* subtitle labels */
    QLabel#HeaderSub {
      font-size: 13px;
      color: #5b6b7c;
    }

    /* section titles */
    QLabel#SectionTitle {
      font-size: 14px;
      font-weight: 600;
      color: #1a2332;
    }

    /* key-value labels */
    QLabel#Key {
      font-size: 12px;
      color: #5b6b7c;
    }

    /* value labels */
    QLabel#Value {
      font-size: 13px;
      font-weight: 600;
      color: #1a2332;
    }

    /* list widgets and plain text edits */
    QListWidget, QPlainTextEdit {
      background: #f8fafc;
      border: 1px solid #e2e8f0;
      border-radius: 6px;
      padding: 6px;
      color: #1a2332;
    }

    /* splitter handles */
    QSplitter::handle {
      background: transparent;
      width: 8px;
      height: 8px;
    }
  )";
} // namespace

MainWindow::MainWindow(QWidget *parent, Server *server) : QMainWindow(parent), server(server) {
  // window properties
  setWindowTitle("Server Window — Distributed Chat Server");
  resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
  setStyleSheet(STYLE_SHEET);

  auto *main = new QWidget(this);     // main display widget
  auto *root = new QVBoxLayout(main); // root layout for the main display widget
  root->setContentsMargins(20, 20, 20, 20);
  root->setSpacing(16);

  // hero widget
  auto *header = buildHeader(main);

  // panels
  ports = new PortsPanel(main, server);
  users = new UsersPanel(main, server);
  rooms = new RoomsPanel(main, server);
  log = new LogPanel(main, server);

  // left side panel
  auto *side = new QSplitter(Qt::Vertical, main);
  side->addWidget(users);
  side->addWidget(rooms);
  side->setStretchFactor(0, 1);
  side->setStretchFactor(1, 1);

  // right side panel
  auto *body = new QSplitter(Qt::Horizontal, main);
  body->addWidget(side);
  body->addWidget(log);
  body->setStretchFactor(0, 1);
  body->setStretchFactor(1, 2);

  // construct the main display (header -> panels -> body)
  root->addWidget(header);
  root->addWidget(ports);
  root->addWidget(body, 1);

  // set the main display widget as the central widget
  setCentralWidget(main);
}

// build the header widget
QWidget *MainWindow::buildHeader(QWidget *parent) {
  auto *header = new QWidget(parent);                   // header widget
  auto *headerLayout = new QVBoxLayout(header);         // header layout
  auto *title = new QLabel("Server dashboard", header); // title label
  auto *subtitle = new QLabel("Live status panels (UI stub — not connected to Server yet)",
                              header); // subtitle label

  // properties
  headerLayout->setContentsMargins(4, 0, 4, 0);
  headerLayout->setSpacing(4);
  title->setObjectName("HeaderTitle");
  subtitle->setObjectName("HeaderSub");
  headerLayout->addWidget(title);
  headerLayout->addWidget(subtitle);
  return header;
}