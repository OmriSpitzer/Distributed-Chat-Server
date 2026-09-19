/**
 * Server log panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"
#include "utils/models/log_message.h"
#include <QPlainTextEdit>
#include <QTimer>

class LogPanel : public Panel {
  Q_OBJECT
public:
  explicit LogPanel(QWidget *parent = nullptr, Server *server = nullptr);
  ~LogPanel() override = default;

private:
  QPlainTextEdit *logTextView{nullptr};
  QTimer *refreshTimer{nullptr};

  void refresh() override;
  void logMessage(const LogMessage &msg);
};
