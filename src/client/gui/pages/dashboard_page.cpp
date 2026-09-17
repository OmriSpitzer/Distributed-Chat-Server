/**
 * Client dashboard page
 *
 * @brief Client dashboard page implementation
 * @date 16-09-2026
 */

#include "client/gui/pages/dashboard_page.h"
#include "client/client.h"
#include "client/gui/components/button.h"
#include "client/gui/components/header.h"
#include "client/gui/components/pop_up_window.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPoint>
#include <QVBoxLayout>

// constructor
DashboardPage::DashboardPage(QWidget *parent, Client *client)
    : Page("Distributed Chat", parent, client) {
  connectHeader();  // connect the header
  buildWorkspace(); // build the workspace

  // body layout properties
  getBodyLayout()->setContentsMargins(20, 20, 20, 20);

  // refresh the page
  refresh();
}

// connect the header
void DashboardPage::connectHeader() {
  // connect buttons
  header()->signUpButton()->setOnClick([this]() { openSignUpDialog(); });
  header()->logInButton()->setOnClick([this]() { openLogInDialog(); });

  // TODO: Examine this code
  header()->userChip()->setOnClick([this]() {
    QMenu menu(this); // create the menu

    menu.addAction("Update profile", this, [this]() { openUpdateProfileDialog(); });
    menu.addSeparator();
    menu.addAction("Logout", this, [this]() { logout(); });
    menu.exec(header()->userChip()->mapToGlobal(QPoint(0, header()->userChip()->height() + 6)));
  });
}

// build the workspace
void DashboardPage::buildWorkspace() {
  auto *split = new QWidget(this);    // create the split widget
  auto *row = new QHBoxLayout(split); // create the row layout

  // row layout properties
  row->setContentsMargins(0, 0, 0, 0);
  row->setSpacing(16);

  // side widget
  auto *side = new QWidget(split);
  side->setObjectName("SideCard"); // set the object name
  side->setFixedWidth(280);        // set the fixed width

  // side layout properties
  auto *sideLayout = new QVBoxLayout(side);       // create the side layout
  sideLayout->setContentsMargins(16, 16, 16, 16); // set the contents margins
  sideLayout->setSpacing(10);                     // set the spacing

  // rooms label
  auto *roomsLabel = new QLabel("ROOMS", side);
  side->setObjectName("SideCard");
  side->setFixedWidth(280);

  // side layout properties
  auto *sideLayout = new QVBoxLayout(side);
  sideLayout->setContentsMargins(16, 16, 16, 16);
  sideLayout->setSpacing(10);

  // rooms list
  auto *roomsLabel = new QLabel("ROOMS", side);
  roomsLabel->setObjectName("SectionTitle");
  roomsList = new QListWidget(side);

  // buttons
  joinButton =
      new Button("Join room", [this]() { openJoinRoomDialog(); }, true, side, "GhostButton");
  leaveButton = new Button("Leave room", [this]() { leaveRoom(); }, true, side, "GhostButton");
  createRoomButton =
      new Button("Create room", [this]() { openCreateRoomDialog(); }, false, side, "GhostButton");
  inviteButton =
      new Button("Invite", [this]() { openInviteDialog(); }, loggedIn, side, "PrimaryButton");

  // room actions
  auto *roomActions = new QHBoxLayout();
  roomActions->addWidget(joinButton, 1);
  roomActions->addWidget(leaveButton, 1);
  auto *roomActions2 = new QHBoxLayout();
  roomActions2->addWidget(createRoomButton, 1);
  roomActions2->addWidget(inviteButton, 1);

  // add widgets to the side layout
  sideLayout->addWidget(roomsLabel);
  sideLayout->addWidget(roomsList, 1);
  sideLayout->addLayout(roomActions);
  sideLayout->addLayout(roomActions2);

  // chat widget
  auto *chat = new QWidget(split);
  chat->setObjectName("ChatCard");

  // chat layout properties
  auto *chatLayout = new QVBoxLayout(chat);
  chatLayout->setContentsMargins(16, 16, 16, 16);
  chatLayout->setSpacing(10);

  // chat header
  auto *chatHeader = new QHBoxLayout();
  roomTitle = new QLabel(chat);
  roomTitle->setObjectName("RoomTitle");

  // buttons
  historyButton =
      new Button("Load history", [this]() { loadHistory(); }, true, chat, "GhostButton");
  updateProfileButton = new Button(
      "Update profile", [this]() { openUpdateProfileDialog(); }, true, chat, "GhostButton");
  chatHeader->addWidget(roomTitle, 1);
  chatHeader->addWidget(historyButton);
  chatHeader->addWidget(updateProfileButton);

  // transcript
  transcript = new QPlainTextEdit(chat);
  transcript->setReadOnly(true);

  // composer row
  auto *composerRow = new QHBoxLayout();
  composer = new QLineEdit(chat);
  composer->setPlaceholderText("Write a message…");
  sendButton = new Button("Send", [this]() { sendMessage(); }, true, chat, "PrimaryButton");
  composerRow->addWidget(composer, 1);
  composerRow->addWidget(sendButton);

  // add widgets to the chat layout
  chatLayout->addLayout(chatHeader);
  chatLayout->addWidget(transcript, 1);
  chatLayout->addLayout(composerRow);

  // add widgets to the row layout
  row->addWidget(side);
  row->addWidget(chat, 1);
  getBodyLayout()->addWidget(split, 1);

  // connect signals
  connect(composer, &QLineEdit::returnPressed, this, &DashboardPage::sendMessage);
  connect(roomsList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
    if (item) {
      currentRoom = item->text().section(' ', 0, 0);
      appendMessage("system", "Joined " + currentRoom);
      refresh();
    }
  });
}

// get the display name
QString DashboardPage::displayName() const { return loggedIn ? username : QString("Guest"); }

// refresh the page
void DashboardPage::refresh() {
  const bool connected = !client() || client()->isAlive(); // check if the client is connected

  // set the subtitle label
  header()->subtitleLabel()->setText(connected ? "Connected" : "Disconnected");

  // set the logged in state
  header()->setLoggedIn(loggedIn, displayName());

  // set the create room button visibility
  createRoomButton->setVisible(loggedIn);

  // clear the rooms list
  roomsList->clear();
  for (const QString &room : rooms) {
    const QString mark = (room == currentRoom) ? "  ·  here" : "";
    roomsList->addItem(room + mark);
  }

  // set the room title
  roomTitle->setText("#  " + currentRoom);
  leaveButton->setEnabled(currentRoom != "Lobby");
}

// append a message to the transcript
void DashboardPage::appendMessage(const QString &author, const QString &text) {
  messages.append(author + ": " + text);
  transcript->appendPlainText(author + "  ·  " + text);
}

// open the log in dialog
void DashboardPage::openLogInDialog() {
  auto *form = new QWidget;              // create the form
  auto *layout = new QFormLayout(form);  // create the form layout
  auto *userField = new QLineEdit(form); // create the username field
  auto *passField = new QLineEdit(form); // create the password field

  // set the password field echo mode
  passField->setEchoMode(QLineEdit::Password);
  layout->addRow("Username", userField);
  layout->addRow("Password", passField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Log in", form, userField)) {
    return;
  }

  // check if the username is empty
  username = userField->text().trimmed();
  if (username.isEmpty()) {
    QMessageBox::warning(this, "Log in", "Username cannot be empty.");
    return;
  }

  // set the email
  email = username + "@example.com";
  loggedIn = true;

  // append a message to the transcript
  appendMessage("system", "Logged in as " + username);
  refresh();
}

// open the sign up dialog
void DashboardPage::openSignUpDialog() {
  auto *form = new QWidget;               // create the form
  auto *layout = new QFormLayout(form);   // create the form layout
  auto *userField = new QLineEdit(form);  // create the username field
  auto *passField = new QLineEdit(form);  // create the password field
  auto *emailField = new QLineEdit(form); // create the email field

  // set the password field echo mode
  passField->setEchoMode(QLineEdit::Password);
  layout->addRow("Username", userField);
  layout->addRow("Password", passField);
  layout->addRow("Email", emailField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Sign in", form, userField)) {
    return;
  }

  // check if the username or email is empty
  username = userField->text().trimmed();
  email = emailField->text().trimmed();
  if (username.isEmpty() || email.isEmpty()) {
    QMessageBox::warning(this, "Sign in", "Username and email are required.");
    return;
  }

  // set the logged in state
  loggedIn = true;
  appendMessage("system", "Account created for " + username);
  refresh();
}

// open the update profile dialog
void DashboardPage::openUpdateProfileDialog() {
  auto *form = new QWidget;                        // create the form
  auto *layout = new QFormLayout(form);            // create the form layout
  auto *userField = new QLineEdit(username, form); // create the username field
  auto *passField = new QLineEdit(form);           // create the password field

  // set the password field echo mode
  passField->setEchoMode(QLineEdit::Password);
  passField->setPlaceholderText("Leave blank to keep current");

  // create the email field
  auto *emailField = new QLineEdit(email, form);
  layout->addRow("Username", userField);
  layout->addRow("New password", passField);
  layout->addRow("Email", emailField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Update profile", form, userField)) {
    return;
  }

  // check if the username is empty
  username = userField->text().trimmed();
  if (username.isEmpty()) {
    QMessageBox::warning(this, "Update profile", "Username cannot be empty.");
    return;
  }

  // set the email
  email = emailField->text().trimmed();
  appendMessage("system", "Profile updated");
  refresh();
}

// open the join room dialog
void DashboardPage::openJoinRoomDialog() {
  auto *form = new QWidget;              // create the form
  auto *layout = new QFormLayout(form);  // create the form layout
  auto *roomField = new QComboBox(form); // create the room field

  // set the room field editable
  roomField->setEditable(true);

  // add the rooms to the room field
  for (const QString &room : rooms) {
    roomField->addItem(room);
  }
  layout->addRow("Room", roomField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Join room", form, roomField->lineEdit())) {
    return;
  }

  // check if the room is empty
  const QString name = roomField->currentText().trimmed();
  if (name.isEmpty()) {
    return;
  }

  // check if the room is already in the list
  if (!rooms.contains(name)) {
    rooms.append(name);
  }

  // set the current room
  currentRoom = name;

  // append a message to the transcript
  appendMessage("system", "Joined " + currentRoom);
  refresh();
}

// open the create room dialog
void DashboardPage::openCreateRoomDialog() {
  // check if the user is logged in
  if (!loggedIn) {
    QMessageBox::information(this, "Create room", "Log in to create a room.");
    return;
  }

  auto *form = new QWidget;              // create the form
  auto *layout = new QFormLayout(form);  // create the form layout
  auto *nameField = new QLineEdit(form); // create the name field
  auto *privacy = new QComboBox(form);   // create the privacy field

  // add the privacy options to the privacy field
  privacy->addItems({"Public", "Private"});
  layout->addRow("Name", nameField);
  layout->addRow("Privacy", privacy);

  // run the pop up window
  if (!PopUpWindow::run(this, "Create room", form, nameField)) {
    return;
  }

  // check if the name is empty
  const QString name = nameField->text().trimmed();
  if (name.isEmpty()) {
    QMessageBox::warning(this, "Create room", "Room name cannot be empty.");
    return;
  }

  // check if the room is already in the list
  if (!rooms.contains(name)) {
    rooms.append(name);
  }

  // set the current room
  currentRoom = name;
  appendMessage("system", "Created " + name + " (" + privacy->currentText() + ")");
  refresh();
}

// open the invite dialog
void DashboardPage::openInviteDialog() {
  auto *form = new QWidget;             // create the form
  auto *layout = new QFormLayout(form); // create the form layout
  auto *invitee = new QLineEdit(form);  // create the invitee field

  // add the room label and invitee field to the form
  layout->addRow("Room", new QLabel(currentRoom, form));
  layout->addRow("Username", invitee);

  // run the pop up window
  if (!PopUpWindow::run(this, "Invite to room", form, invitee)) {
    return;
  }

  // check if the invitee is empty
  const QString username = invitee->text().trimmed();
  if (username.isEmpty()) {
    QMessageBox::warning(this, "Invite", "Username cannot be empty.");
    return;
  }

  // append a message to the transcript
  appendMessage("system", "Invited " + username + " to " + currentRoom);
}

// leave the room
void DashboardPage::leaveRoom() {
  // check if the current room is the lobby
  if (currentRoom == "Lobby") {
    QMessageBox::information(this, "Leave room", "Cannot leave the Lobby.");
    return;
  }

  // set the current room to the lobby
  const QString left = currentRoom;
  currentRoom = "Lobby";
  appendMessage("system", "Left " + left);
  refresh();
}

// load the history
void DashboardPage::loadHistory() {
  appendMessage("system", "History for " + currentRoom);
  transcript->appendPlainText("  (no messages — stub)");
}

// send a message
void DashboardPage::sendMessage() {
  const QString text = composer->text().trimmed(); // get the text from the composer

  // check if the text is empty
  if (text.isEmpty()) {
    return;
  }

  // append a message to the transcript
  appendMessage(displayName(), text);
  composer->clear();
}

// logout
void DashboardPage::logout() {
  loggedIn = false;
  username.clear();
  email.clear();
  appendMessage("system", "Logged out");
  refresh();
}
