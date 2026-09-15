/**
 * Server log panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"

class QPlainTextEdit;
class QTimer;

class LogPanel : public Panel {
  Q_OBJECT
public:
  explicit LogPanel(QWidget *parent = nullptr, Server *server = nullptr);

  void refresh() override;
  void appendLine(const QString &line);

private:
  QPlainTextEdit *logView_{nullptr};
  QTimer *refreshTimer_{nullptr};
};
