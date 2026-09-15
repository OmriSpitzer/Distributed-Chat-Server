/**
 * Server log panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/log_panel.h"
#include "utils/models/logger.h"
#include <QDateTime>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTimer>

LogPanel::LogPanel(QWidget *parent, Server *server) : Panel("Server log", parent, server) {
  logTextView = new QPlainTextEdit(this);
  refreshTimer = new QTimer(this);

  logTextView->setReadOnly(true);
  logTextView->setPlaceholderText("Retrieving log messages...");
  getBodyLayout()->addWidget(logTextView, 1);

  connect(refreshTimer, &QTimer::timeout, this, &LogPanel::refresh);
  refreshTimer->start(REFRESH_INTERVAL);
  refresh();
}

void LogPanel::refresh() {
  Logger &logger = Logger::getInstance();
  const std::size_t n = logger.size();
  const int verticalValue = logTextView->verticalScrollBar()->value();
  const int verticalMax = logTextView->verticalScrollBar()->maximum();
  const bool stickToBottom = verticalValue == verticalMax;

  logTextView->clear();
  for (std::size_t i = 0; i < n; ++i) {
    try {
      logMessage(logger.getMessage(i));
    } catch (...) {
      break;
    }
  }

  if (stickToBottom) {
    logTextView->verticalScrollBar()->setValue(logTextView->verticalScrollBar()->maximum());
  }
}

void LogPanel::logMessage(const LogMessage &msg) {
  const QString line =
      QString("[%1] (%2) %3: %4")
          .arg(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(msg.getTimestamp()))
                   .toString("hh:mm:ss"))
          .arg(QString::fromStdString(LogMessage::typeToString(msg.getType())))
          .arg(QString::fromStdString(msg.getSource()))
          .arg(QString::fromStdString(msg.getMessage()));
  logTextView->appendPlainText(line);
}
