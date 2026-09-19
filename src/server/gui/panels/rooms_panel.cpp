/**
 * Rooms panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/rooms_panel.h"
#include "server/room_manager.h"
#include <QListWidget>
#include <QTimer>

RoomsPanel::RoomsPanel(QWidget *parent, Server *server) : Panel("Rooms", parent, server) {
  roomsList = new QListWidget(this);
  refreshTimer = new QTimer(this);

  getBodyLayout()->addWidget(roomsList, 1);

  connect(refreshTimer, &QTimer::timeout, this, &RoomsPanel::refresh);
  refreshTimer->start(REFRESH_INTERVAL);
  refresh();
}

void RoomsPanel::refresh() {
  roomsList->clear();

  const std::vector<Room> rooms = RoomManager::getInstance().listRooms();
  if (rooms.empty()) {
    roomsList->addItem("(no rooms)");
    return;
  }

  for (const Room &room : rooms) {
    addRoom(room);
  }
}

void RoomsPanel::addRoom(const Room &room) {
  const QString formatted =
      QString("%1 — %2 / %3")
          .arg(QString::fromStdString(room.getName()))
          .arg(QString::fromStdString(Room::roomTypeToString(room.getType())))
          .arg(QString::fromStdString(Room::privacyToString(room.getPrivacy())));
  roomsList->addItem(formatted);
}
