/**
 * Node & ports status panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/ports_panel.h"
#include "config/config.h"
#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <sstream>

namespace {

QLabel *makeValue(QWidget *parent) {
  auto *label = new QLabel("—", parent);
  label->setObjectName("Value");
  label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  return label;
}

QLabel *makeKey(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);
  label->setObjectName("Key");
  return label;
}

} // namespace

PortsPanel::PortsPanel(QWidget *parent, Server *server) : Panel("Node & ports", parent, server) {
  nodeLabel = makeValue(this);
  clientLabel = makeValue(this);
  peerLabel = makeValue(this);
  peersLabel = makeValue(this);
  dbLabel = makeValue(this);
  auto *grid = new QGridLayout();
  refreshTimer = new QTimer(this);

  grid->setHorizontalSpacing(24);
  grid->setVerticalSpacing(8);

  grid->addWidget(makeKey("Node ID", this), 0, 0);
  grid->addWidget(nodeLabel, 0, 1);
  grid->addWidget(makeKey("Client port", this), 1, 0);
  grid->addWidget(clientLabel, 1, 1);
  grid->addWidget(makeKey("Peer port", this), 2, 0);
  grid->addWidget(peerLabel, 2, 1);
  grid->addWidget(makeKey("Peers", this), 3, 0);
  grid->addWidget(peersLabel, 3, 1);
  grid->addWidget(makeKey("Database", this), 4, 0);
  grid->addWidget(dbLabel, 4, 1);
  grid->setColumnStretch(1, 1);

  getBodyLayout()->addLayout(grid);

  connect(refreshTimer, &QTimer::timeout, this, &PortsPanel::refresh);
  refreshTimer->start(REFRESH_INTERVAL);
  refresh();
}

void PortsPanel::refresh() {
  nodeLabel->setText(QString::fromStdString(config::NODE_ID));
  clientLabel->setText(QString::number(config::PORT));
  peerLabel->setText(QString::number(config::PEER_PORT));
  dbLabel->setText(QString::fromStdString(config::DB_PATH));

  if (config::PEERS.empty()) {
    peersLabel->setText("(none)");
    return;
  }

  std::ostringstream oss;
  for (std::size_t i = 0; i < config::PEERS.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << config::PEERS[i];
  }
  peersLabel->setText(QString::fromStdString(oss.str()));
}
