/**
 * Node / ports status panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/ports_panel.h"
#include "config/config.h"
#include <QGridLayout>
#include <QLabel>
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

PortsPanel::PortsPanel(QWidget *parent, Server *server)
    : Panel("Node & ports", parent, server) {
  nodeLabel_ = makeValue(this);
  clientPortLabel_ = makeValue(this);
  peerPortLabel_ = makeValue(this);
  peersLabel_ = makeValue(this);
  dbLabel_ = makeValue(this);

  auto *grid = new QGridLayout();
  grid->setHorizontalSpacing(24);
  grid->setVerticalSpacing(8);
  grid->addWidget(makeKey("Node ID", this), 0, 0);
  grid->addWidget(nodeLabel_, 0, 1);
  grid->addWidget(makeKey("Client port", this), 1, 0);
  grid->addWidget(clientPortLabel_, 1, 1);
  grid->addWidget(makeKey("Peer port", this), 2, 0);
  grid->addWidget(peerPortLabel_, 2, 1);
  grid->addWidget(makeKey("Peers", this), 3, 0);
  grid->addWidget(peersLabel_, 3, 1);
  grid->addWidget(makeKey("Database", this), 4, 0);
  grid->addWidget(dbLabel_, 4, 1);
  grid->setColumnStretch(1, 1);

  bodyLayout()->addLayout(grid);
  refresh();
}

void PortsPanel::refresh() {
  nodeLabel_->setText(QString::fromStdString(config::NODE_ID));
  clientPortLabel_->setText(QString::number(config::PORT));
  peerPortLabel_->setText(QString::number(config::PEER_PORT));
  dbLabel_->setText(QString::fromStdString(config::DB_PATH));

  if (config::PEERS.empty()) {
    peersLabel_->setText("(none)");
    return;
  }

  std::ostringstream oss;
  for (std::size_t i = 0; i < config::PEERS.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << config::PEERS[i];
  }
  peersLabel_->setText(QString::fromStdString(oss.str()));
}
