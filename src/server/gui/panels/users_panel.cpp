/**
 * Online users panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/users_panel.h"
#include "server/server.h"
#include <QListWidget>
#include <QTimer>

UsersPanel::UsersPanel(QWidget *parent, Server *server)
    : Panel("Online users", parent, server) {
  usersList = new QListWidget(this);
  bodyLayout()->addWidget(usersList, 1);

  refreshTimer = new QTimer(this);
  connect(refreshTimer, &QTimer::timeout, this, &UsersPanel::refresh);
  refreshTimer->start(1000);
  refresh();
}

void UsersPanel::refresh() {
  usersList->clear();

  if (!server()) {
    usersList->addItem("(no server)");
    return;
  }

  const auto sessions = server()->connections().getSessions();
  if (sessions.empty()) {
    usersList->addItem("(no sessions)");
    return;
  }

  for (const auto &[sock, session] : sessions) {
    if (!session) {
      continue;
    }
    if (!session->isAuthenticated()) {
      usersList->addItem(QString("socket %1 (unauthed)").arg(quintptr(sock)));
      continue;
    }
    const User user = session->getUser();
    const Room room = session->getRoom();
    usersList->addItem(QString("%1 @ %2")
                           .arg(QString::fromStdString(user.getUsername()))
                           .arg(QString::fromStdString(room.getName())));
  }
}
