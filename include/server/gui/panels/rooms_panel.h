/**
 * Rooms panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"

class QListWidget;
class QTimer;

class RoomsPanel : public Panel {
  Q_OBJECT
public:
  explicit RoomsPanel(QWidget *parent = nullptr, Server *server = nullptr);

  void refresh() override;

private:
  QListWidget *list_{nullptr};
  QTimer *refreshTimer_{nullptr};
};
