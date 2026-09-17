/**
 * Abstract client GUI page base implementation
 *
 * @brief Abstract page base class implementation
 * @date 16-09-2026
 */

#include "client/gui/components/page.h"
#include "client/gui/components/header.h"
#include <QVBoxLayout>

Page::Page(const QString &title, QWidget *parent, Client *client)
    : QWidget(parent), clientObject(client) {
  setObjectName("Page");                  // set the object name to "Page"
  rootLayout = new QVBoxLayout(this);     // create the root layout
  headerObject = new Header(title, this); // create the header object
  bodyLayout = new QVBoxLayout();         // create the body layout

  // root layout properties
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->setSpacing(0);

  // body layout properties
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(0);

  // add header and body layout to the root layout
  rootLayout->addWidget(headerObject);
  rootLayout->addLayout(bodyLayout, 1);
}
