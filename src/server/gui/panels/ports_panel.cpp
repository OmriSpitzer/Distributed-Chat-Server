/**
 * Node & ports status panel implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/ports_panel.h"
#include "config/config.h"
#include "server/server.h"
#include "utils/health/i_health_check.h"
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
  endpointsLabel = makeValue(this);
  dbLabel = makeValue(this);
  healthOverall = makeValue(this);
  healthDetails = makeValue(this);
  healthDetails->setWordWrap(true);
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
  grid->addWidget(makeKey("Gossip seeds", this), 3, 0);
  grid->addWidget(peersLabel, 3, 1);
  grid->addWidget(makeKey("Client endpoints", this), 4, 0);
  grid->addWidget(endpointsLabel, 4, 1);
  grid->addWidget(makeKey("Database", this), 5, 0);
  grid->addWidget(dbLabel, 5, 1);
  grid->addWidget(makeKey("Health", this), 6, 0);
  grid->addWidget(healthOverall, 6, 1);
  grid->addWidget(makeKey("Checks", this), 7, 0);
  grid->addWidget(healthDetails, 7, 1);
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
  } else {
    std::ostringstream oss;
    for (std::size_t i = 0; i < config::PEERS.size(); ++i) {
      if (i > 0) {
        oss << ", ";
      }
      oss << config::PEERS[i];
    }
    peersLabel->setText(QString::fromStdString(oss.str()));
  }

  if (server() == nullptr) {
    endpointsLabel->setText("—");
    healthOverall->setText("—");
    healthDetails->setText("—");
    return;
  }

  {
    const auto live = server()->gossip().getClientPeers();
    if (live.empty()) {
      endpointsLabel->setText("(none)");
    } else {
      std::ostringstream oss;
      bool first = true;
      for (const auto &entry : live) {
        if (!first) {
          oss << ", ";
        }
        first = false;
        oss << entry.second.nodeId << "=" << entry.second.host << ":" << entry.second.port;
      }
      endpointsLabel->setText(QString::fromStdString(oss.str()));
    }
  }

  // one pass: per-check lines + overall (avoid double ping via check() + checkAll())
  const auto reports = server()->health().checkAll();
  HealthStatus overallStatus = HealthStatus::Up;
  QStringList lines;
  QStringList downDetails;
  for (const auto &report : reports) {
    QString line = QString::fromStdString(report.name) + ": " +
                   QString::fromStdString(report.statusToString());
    if (!report.detail.empty()) {
      line += " (" + QString::fromStdString(report.detail) + ")";
    }
    lines << line;
    if (report.status == HealthStatus::Down) {
      overallStatus = HealthStatus::Down;
      downDetails << QString::fromStdString(report.name) + ":Down";
    } else if (report.status == HealthStatus::Degraded && overallStatus != HealthStatus::Down) {
      overallStatus = HealthStatus::Degraded;
    }
  }
  healthOverall->setText(QString::fromStdString(healthStatusToString(overallStatus)) +
                         (downDetails.isEmpty() ? QString() : " (" + downDetails.join("; ") + ")"));
  healthDetails->setText(lines.isEmpty() ? "(none)" : lines.join("\n"));
}
