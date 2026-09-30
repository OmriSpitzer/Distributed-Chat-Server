/**
 * Server log panel implementation
 *
 * @brief Live updates via QtLogger signals
 * @date 15-09-2026
 */

#include "server/gui/panels/log_panel.h"
#include "utils/logger/logger.h"
#include "utils/logger/qtLogger.h"
#include <QDateTime>
#include <QPlainTextEdit>
#include <QScrollBar>

namespace {
QtLogger &qtLoggerInstance() {
  static QtLogger instance;
  return instance;
}
} // namespace

LogPanel::LogPanel(QWidget *parent, Server *server) : Panel("Server log", parent, server) {
  logTextView = new QPlainTextEdit(this);

  logTextView->setReadOnly(true);
  logTextView->setPlaceholderText("Retrieving log messages...");
  getBodyLayout()->addWidget(logTextView, 1);

  QtLogger &qtLogger = qtLoggerInstance();
  Logger::getInstance().addLogger(&qtLogger);
  connect(&qtLogger, &QtLogger::logLine, this, &LogPanel::appendLine, Qt::QueuedConnection);

  refresh(); // one-shot backlog from the in-memory ring
}

// append the log line to the UI
void LogPanel::appendLine(const QString &line) {
  const int verticalValue = logTextView->verticalScrollBar()->value();
  const int verticalMax = logTextView->verticalScrollBar()->maximum();
  const bool stickToBottom = verticalValue == verticalMax;

  logTextView->appendPlainText(line);

  if (stickToBottom) {
    logTextView->verticalScrollBar()->setValue(logTextView->verticalScrollBar()->maximum());
  }
}

// refresh the log panel
void LogPanel::refresh() {
  Logger &logger = Logger::getInstance();
  const std::size_t n = logger.size();

  logTextView->clear();
  for (std::size_t i = 0; i < n; ++i) {
    try {
      logMessage(logger.getMessage(i));
    } catch (...) {
      break;
    }
  }

  logTextView->verticalScrollBar()->setValue(logTextView->verticalScrollBar()->maximum());
}

// log the message to the UI
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
