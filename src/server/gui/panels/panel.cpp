/**
 * Abstract server GUI panel base implementation
 *
 * @date 15-09-2026
 */

#include "server/gui/panels/panel.h"
#include <QLabel>
#include <QVBoxLayout>

Panel::Panel(const QString &title, QWidget *parent, Server *server)
    : QWidget(parent), server_(server) {
  setObjectName("Panel");

  rootLayout_ = new QVBoxLayout(this);
  rootLayout_->setContentsMargins(16, 16, 16, 16);
  rootLayout_->setSpacing(10);

  titleLabel_ = new QLabel(title, this);
  titleLabel_->setObjectName("SectionTitle");
  rootLayout_->addWidget(titleLabel_);

  bodyLayout_ = new QVBoxLayout();
  bodyLayout_->setContentsMargins(0, 0, 0, 0);
  bodyLayout_->setSpacing(10);
  rootLayout_->addLayout(bodyLayout_, 1);
}
