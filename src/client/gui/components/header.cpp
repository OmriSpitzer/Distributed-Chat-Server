/**
 * Client GUI page header implementation
 *
 * @brief Page header widget implementation
 * @date 17-09-2026
 */

#include "client/gui/components/header.h"
#include "client/gui/components/button.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

Header::Header(const QString &title, QWidget *parent) : QWidget(parent) {
  setObjectName("Header");            // set the object name to "Header"
  auto *root = new QHBoxLayout(this); // create the root layout
  auto *titles = new QVBoxLayout();   // create the titles layout
  auto *right = new QHBoxLayout();    // create the right layout

  headingLabel = new QLabel(title, this);
  headingSubLabel = new QLabel(this);
  signUpBtn = new Button("Sign in", nullptr, true, this, "GhostButton");
  logInBtn = new Button("Log in", nullptr, true, this, "PrimaryButton");
  userChipBtn = new Button("Guest", nullptr, false, this, "UserChip");

  // root layout properties
  root->setContentsMargins(24, 16, 24, 16);
  root->setSpacing(16);

  // titles layout properties
  titles->setContentsMargins(0, 0, 0, 0);
  titles->setSpacing(2);

  // labels properties
  headingLabel->setObjectName("PageTitle");
  headingSubLabel->setObjectName("PageSubtitle");

  // add labels to the titles layout
  titles->addWidget(headingLabel);
  titles->addWidget(headingSubLabel);

  // right layout properties
  right->setContentsMargins(0, 0, 0, 0);
  right->setSpacing(8);
  right->addWidget(signUpBtn);
  right->addWidget(logInBtn);
  right->addWidget(userChipBtn);

  // add layouts to the root layout
  root->addLayout(titles, 1);
  root->addLayout(right);
}

// set the logged in state and username
void Header::setLoggedIn(bool loggedIn, const QString &username) {
  signUpBtn->setVisible(!loggedIn);
  logInBtn->setVisible(!loggedIn);
  userChipBtn->setVisible(loggedIn);
  userChipBtn->setText(loggedIn ? username : QString("Guest"));
}
