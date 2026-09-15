/**
 * Rooms panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"
#include "utils/models/room.h"
#include <QListWidget>
#include <QTimer>

class RoomsPanel : public Panel {
  Q_OBJECT
public:
  explicit RoomsPanel(QWidget *parent = nullptr, Server *server = nullptr);
  ~RoomsPanel() override = default;

private:
  QListWidget *roomsList{nullptr};
  QTimer *refreshTimer{nullptr};

  void refresh() override;
  void addRoom(const Room &room);
};
