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

LogPanel::LogPanel(QWidget *parent, Server *server)
    : Panel("Server log", parent, server) {
  logView_ = new QPlainTextEdit(this);
  logView_->setReadOnly(true);
  logView_->setPlaceholderText("Log output will appear here…");
  bodyLayout()->addWidget(logView_, 1);

  refreshTimer_ = new QTimer(this);
  connect(refreshTimer_, &QTimer::timeout, this, &LogPanel::refresh);
  refreshTimer_->start(500);
  refresh();
}

void LogPanel::refresh() {
  const std::size_t n = Logger::size();
  const bool stickToBottom =
      logView_->verticalScrollBar()->value() == logView_->verticalScrollBar()->maximum();

  // Full rebuild stays correct when Logger drops oldest messages
  logView_->clear();
  Logger &logger = Logger::getInstance();
  for (std::size_t i = 0; i < n; ++i) {
    try {
      const LogMessage msg = logger.getMessage(i);
      logView_->appendPlainText(
          QString("[%1] (%2) %3: %4")
              .arg(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(msg.getTimestamp()))
                       .toString("hh:mm:ss"))
              .arg(QString::fromStdString(LogMessage::typeToString(msg.getType())))
              .arg(QString::fromStdString(msg.getSource()))
              .arg(QString::fromStdString(msg.getMessage())));
    } catch (...) {
      break;
    }
  }

  if (stickToBottom) {
    logView_->verticalScrollBar()->setValue(logView_->verticalScrollBar()->maximum());
  }
}

void LogPanel::appendLine(const QString &line) { logView_->appendPlainText(line); }
