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
  setObjectName("Header");
  auto *root = new QHBoxLayout(this);
  auto *titles = new QVBoxLayout();
  auto *right = new QHBoxLayout();

  headingLabel = new QLabel(title, this);
  headingSubLabel = new QLabel(this);
  themeBtn = new Button("Light", nullptr, true, this, "GhostButton");
  densityBtn = new Button("Comfortable", nullptr, true, this, "GhostButton");
  signUpBtn = new Button("Sign in", nullptr, true, this, "GhostButton");
  logInBtn = new Button("Log in", nullptr, true, this, "PrimaryButton");
  userChipBtn = new Button("Guest", nullptr, false, this, "UserChip");
  logoutBtn = new Button("Logout", nullptr, false, this, "GhostButton");

  rootLayout = root;
  root->setContentsMargins(24, 16, 24, 16);
  root->setSpacing(16);

  titles->setContentsMargins(0, 0, 0, 0);
  titles->setSpacing(2);

  headingLabel->setObjectName("PageTitle");
  headingSubLabel->setObjectName("PageSubtitle");

  titles->addWidget(headingLabel);
  titles->addWidget(headingSubLabel);

  right->setContentsMargins(0, 0, 0, 0);
  right->setSpacing(8);
  right->addWidget(themeBtn);
  right->addWidget(densityBtn);
  right->addWidget(signUpBtn);
  right->addWidget(logInBtn);
  right->addWidget(userChipBtn);
  right->addWidget(logoutBtn);

  root->addLayout(titles, 1);
  root->addLayout(right);
}

// assign header padding from the caller's spacing numbers
void Header::applySpacing(int marginH, int marginV, int spacing) {
  if (!rootLayout) {
    return;
  }
  rootLayout->setContentsMargins(marginH, marginV, marginH, marginV);
  rootLayout->setSpacing(spacing);
}

// set the logged in state and username
void Header::setLoggedIn(bool loggedIn, const QString &username) {
  signUpBtn->setVisible(!loggedIn);
  logInBtn->setVisible(!loggedIn);
  userChipBtn->setVisible(loggedIn);
  logoutBtn->setVisible(loggedIn);
  userChipBtn->setText(loggedIn ? username : QString("Guest"));
}
