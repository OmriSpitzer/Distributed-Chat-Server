/**
 * Online users panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"

class QListWidget;
class QTimer;

class UsersPanel : public Panel {
  Q_OBJECT
public:
  explicit UsersPanel(QWidget *parent = nullptr, Server *server = nullptr);

  void refresh() override;

private:
  QListWidget *usersList{nullptr};
  QTimer *refreshTimer{nullptr};
};
