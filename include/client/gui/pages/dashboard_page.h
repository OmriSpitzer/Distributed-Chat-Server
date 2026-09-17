/**
 * Client dashboard page (UI stub)
 *
 * @date 16-09-2026
 */

#pragma once
#include "client/gui/components/page.h"
#include <QString>
#include <QStringList>

class Button;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;

class DashboardPage : public Page {
  Q_OBJECT
public:
  // constructor
  explicit DashboardPage(QWidget *parent = nullptr, Client *client = nullptr);

  // destructor
  ~DashboardPage() override = default;

private:
  // rooms
  QListWidget *roomsList{nullptr};        // rooms list widget
  Button *joinButton{nullptr};            // join button
  Button *leaveButton{nullptr};           // leave button
  Button *createRoomButton{nullptr};      // create room button
  Button *inviteButton{nullptr};          // invite button

  // chat
  QLabel *roomTitle{nullptr};           // room title label
  Button *historyButton{nullptr};       // history button
  Button *updateProfileButton{nullptr}; // update profile button
  QPlainTextEdit *transcript{nullptr};  // transcript plain text edit
  QLineEdit *composer{nullptr};         // composer line edit
  Button *sendButton{nullptr};          // send button

  // stub session (not wired to Client yet)
  bool loggedIn{false};
  QString username;
  QString email;
  QString currentRoom{"Lobby"};
  QStringList rooms{"Lobby", "General", "Research"};
  QStringList messages;

  // connect the header
  void connectHeader();

  // build the workspace
  void buildWorkspace();

  // refresh the page
  void refresh() override;

  // append a message to the transcript
  void appendMessage(const QString &author, const QString &text);

  // get the display name
  QString displayName() const;

  // open the log in dialog
  void openLogInDialog();

  // open the sign up dialog
  void openSignUpDialog();

  // open the update profile dialog
  void openUpdateProfileDialog();

  // open the join room dialog
  void openJoinRoomDialog();

  // open the create room dialog
  void openCreateRoomDialog();

  // open the invite dialog
  void openInviteDialog();

  // leave the room
  void leaveRoom();

  // load the history
  void loadHistory();

  // send a message
  void sendMessage();

  // logout
  void logout();
};
