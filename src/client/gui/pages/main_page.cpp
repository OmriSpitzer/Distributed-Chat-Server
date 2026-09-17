/**
 * Client GUI entry point
 *
 * @date 16-09-2026
 */

#include "client/gui/pages/main_page.h"
#include "client/gui/pages/dashboard_page.h"
#include <QApplication>
#include <QMainWindow>

namespace {
const char *STYLE_SHEET = R"(
    QMainWindow { background: #eef1f6; }

    QWidget#Page { background: #eef1f6; }

    QWidget#Header {
      background: #ffffff;
      border-bottom: 1px solid #e2e8f0;
    }

    QLabel#PageTitle {
      font-size: 20px;
      font-weight: 700;
      color: #0f172a;
    }

    QLabel#PageSubtitle {
      font-size: 12px;
      color: #64748b;
    }

    QPushButton#GhostButton {
      background: transparent;
      border: 1px solid #cbd5e1;
      border-radius: 8px;
      padding: 8px 14px;
      color: #334155;
      font-weight: 600;
    }
    QPushButton#GhostButton:hover { background: #f8fafc; }

    QPushButton#PrimaryButton {
      background: #4f46e5;
      border: none;
      border-radius: 8px;
      padding: 8px 16px;
      color: #ffffff;
      font-weight: 600;
    }
    QPushButton#PrimaryButton:hover { background: #4338ca; }
    QPushButton#PrimaryButton:disabled { background: #c7d2fe; }

    QPushButton#UserChip {
      background: #eef2ff;
      border: 1px solid #c7d2fe;
      border-radius: 18px;
      padding: 6px 14px;
      color: #3730a3;
      font-weight: 600;
    }
    QPushButton#UserChip:hover { background: #e0e7ff; }

    QWidget#SideCard, QWidget#ChatCard {
      background: #ffffff;
      border: 1px solid #e2e8f0;
      border-radius: 14px;
    }

    QLabel#SectionTitle {
      font-size: 12px;
      font-weight: 700;
      letter-spacing: 0.6px;
      color: #64748b;
    }

    QLabel#RoomTitle {
      font-size: 16px;
      font-weight: 700;
      color: #0f172a;
    }

    QListWidget, QPlainTextEdit, QLineEdit {
      background: #f8fafc;
      border: 1px solid #e2e8f0;
      border-radius: 10px;
      padding: 8px;
      color: #0f172a;
      selection-background-color: #c7d2fe;
    }

    QListWidget::item { padding: 8px; border-radius: 8px; }
    QListWidget::item:selected { background: #eef2ff; color: #3730a3; }
  )";
} // namespace

int MainPage::run(int argc, char *argv[], Client &client) {
  // create the application
  QApplication app(argc, argv);
  app.setStyleSheet(STYLE_SHEET);

  // create the main window
  QMainWindow window;
  window.setWindowTitle("Distributed Chat");
  window.resize(1200, 760);
  window.setCentralWidget(new DashboardPage(&window, &client));

  // show the window
  window.show();
  return app.exec();
}
