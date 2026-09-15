/**
 * Abstract server GUI panel base implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/panel.h"
#include <QLabel>
#include <QVBoxLayout>

Panel::Panel(const QString &title, QWidget *parent, Server *server)
    : QWidget(parent), serverObject(server) {
  setObjectName("Panel");

  rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(16, 16, 16, 16);
  rootLayout->setSpacing(10);

  titleLabel = new QLabel(title, this);
  titleLabel->setObjectName("SectionTitle");
  rootLayout->addWidget(titleLabel);

  bodyLayout = new QVBoxLayout();
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(10);
  rootLayout->addLayout(bodyLayout, 1);
}
