/**
 * Node & ports status panel
 *
 * @date 15-09-2026
 */

#pragma once
#include "server/gui/panels/panel.h"
#include <QLabel>
#include <QTimer>

class PortsPanel : public Panel {
  Q_OBJECT
public:
  // constructor
  explicit PortsPanel(QWidget *parent = nullptr, Server *server = nullptr);

  // destructor
  ~PortsPanel() override = default;

private:
  QLabel *nodeLabel{nullptr};     // node label
  QLabel *clientLabel{nullptr};   // client label
  QLabel *peerLabel{nullptr};     // peer label
  QLabel *peersLabel{nullptr};    // peers label
  QLabel *dbLabel{nullptr};       // db label
  QLabel *healthOverall{nullptr}; // overall health
  QLabel *healthDetails{nullptr}; // per-check health lines
  QTimer *refreshTimer{nullptr};  // timer for refreshing the ports

  // refresh panel contents
  void refresh() override;
};
