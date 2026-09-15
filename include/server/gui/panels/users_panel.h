/**
 * Online users panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <QListWidget>
#include <QTimer>

class UsersPanel : public Panel {
  Q_OBJECT
public:
  explicit UsersPanel(QWidget *parent = nullptr, Server *server = nullptr);
  ~UsersPanel() override = default;

private:
  QListWidget *usersList{nullptr};
  QTimer *refreshTimer{nullptr};

  void refresh() override;
  void addUser(const User &user, const Room &room, bool authenticated);
};
