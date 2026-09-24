/**
 * Server log panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"
#include "utils/logger/log_message.h"
#include <QPlainTextEdit>
#include <QString>

class LogPanel : public Panel {
  Q_OBJECT
public:
  explicit LogPanel(QWidget *parent = nullptr, Server *server = nullptr);
  ~LogPanel() override = default;

private slots:
  void appendLine(const QString &line);

private:
  QPlainTextEdit *logTextView{nullptr};

  void refresh() override;
  void logMessage(const LogMessage &msg);
};
