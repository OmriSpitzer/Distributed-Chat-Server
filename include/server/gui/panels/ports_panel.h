/**
 * Node / ports status panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"

class QLabel;

class PortsPanel : public Panel {
  Q_OBJECT
public:
  explicit PortsPanel(QWidget *parent = nullptr, Server *server = nullptr);

  void refresh() override;

private:
  QLabel *nodeLabel_{nullptr};
  QLabel *clientPortLabel_{nullptr};
  QLabel *peerPortLabel_{nullptr};
  QLabel *peersLabel_{nullptr};
  QLabel *dbLabel_{nullptr};
};
