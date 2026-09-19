/**
 * Abstract client GUI page base implementation
 *
 * @brief Abstract page base class implementation
 * @date 16-09-2026
 */

#include "client/gui/components/page.h"
#include "client/gui/components/header.h"
#include <QTimer>
#include <QVBoxLayout>

// constructor
Page::Page(const QString &title, QWidget *parent, Client *client)
    : QWidget(parent), clientObject(client) {
  setObjectName("Page");                  // set the object name to "Page"
  rootLayout = new QVBoxLayout(this);     // create the root layout
  headerObject = new Header(title, this); // create the header object
  bodyLayout = new QVBoxLayout();         // create the body layout
  refreshTimer = new QTimer(this);        // create the refresh timer

  // root layout properties
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  // body layout properties
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(0);

  // add header and body layout to the root layout
  rootLayout->addWidget(headerObject);
  rootLayout->addLayout(bodyLayout, 1);

  // lambda so virtual refresh() dispatches to the concrete page
  connect(refreshTimer, &QTimer::timeout, this, [this]() { refresh(); });
  refreshTimer->start(REFRESH_INTERVAL);
}
