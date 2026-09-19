/**
 * Online users panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/users_panel.h"
#include "server/server.h"
#include <QListWidget>
#include <QTimer>

UsersPanel::UsersPanel(QWidget *parent, Server *server) : Panel("Online users", parent, server) {
  usersList = new QListWidget(this);
  refreshTimer = new QTimer(this);

  getBodyLayout()->addWidget(usersList, 1);

  connect(refreshTimer, &QTimer::timeout, this, &UsersPanel::refresh);
  refreshTimer->start(REFRESH_INTERVAL);
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
    (void)sock;
    if (!session) {
      continue;
    }
    addUser(session->getUser(), session->getRoom(), session->isAuthenticated());
  }
}

void UsersPanel::addUser(const User &user, const Room &room, bool authenticated) {
  QString formatted = QString("%1 @ %2")
                          .arg(QString::fromStdString(user.getUsername()))
                          .arg(QString::fromStdString(room.getName()));
  if (!authenticated) {
    formatted += " (un-authenticated)";
  }
  usersList->addItem(formatted);
}
