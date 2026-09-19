/**
 * Client GUI page header
 *
 * @date 17-09-2026
 */

#pragma once
#include <QString>
#include <QWidget>

class Button;
class QLabel;

class Header : public QWidget {
  Q_OBJECT
public:
  // constructor
  explicit Header(const QString &title, QWidget *parent = nullptr);

  // destructor
  ~Header() override = default;

  // get the title label
  QLabel *titleLabel() const { return headingLabel; }

  // get the subtitle label
  QLabel *subtitleLabel() const { return headingSubLabel; }

  // buttons
  Button *signUpButton() const { return signUpBtn; }
  Button *logInButton() const { return logInBtn; }
  Button *userChip() const { return userChipBtn; }
  Button *logoutButton() const { return logoutBtn; }

  // show Sign in / Log in, or the user chip + logout
  void setLoggedIn(bool loggedIn, const QString &username = QString());

private:
  QLabel *headingLabel{nullptr};    // page title
  QLabel *headingSubLabel{nullptr}; // page subtitle
  Button *signUpBtn{nullptr};       // Sign in
  Button *logInBtn{nullptr};        // Log in
  Button *userChipBtn{nullptr};     // logged-in username chip
  Button *logoutBtn{nullptr};       // Logout (next to chip)
};
