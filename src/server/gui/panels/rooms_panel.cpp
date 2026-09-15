/**
 * Rooms panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/rooms_panel.h"
#include "server/room_manager.h"
#include <QListWidget>
#include <QTimer>

RoomsPanel::RoomsPanel(QWidget *parent, Server *server)
    : Panel("Rooms", parent, server) {
  list_ = new QListWidget(this);
  bodyLayout()->addWidget(list_, 1);

  refreshTimer_ = new QTimer(this);
  connect(refreshTimer_, &QTimer::timeout, this, &RoomsPanel::refresh);
  refreshTimer_->start(1000);
  refresh();
}

void RoomsPanel::refresh() {
  list_->clear();

  const std::vector<Room> rooms = RoomManager::getInstance().listRooms();
  if (rooms.empty()) {
    list_->addItem("(no rooms)");
    return;
  }

  for (const Room &room : rooms) {
    list_->addItem(
        QString("%1 — %2 / %3")
            .arg(QString::fromStdString(room.getName()))
            .arg(QString::fromStdString(Room::roomTypeToString(room.getType())))
            .arg(QString::fromStdString(Room::privacyToString(room.getPrivacy()))));
  }
}
