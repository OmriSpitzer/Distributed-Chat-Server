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
#include "utils/models/room.h"
#include "utils/models/user.h"
#include <QComboBox>
#include <QDateTime>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QVariant>
#include <QVBoxLayout>
#include <algorithm>
#include <vector>

namespace {
QString roomNameOf(const ClientState &state) {
  if (!state.currentRoom) {
    return QStringLiteral("Lobby");
  }
  return QString::fromStdString(state.currentRoom->getName());
}

QString formatChatTime(quint64 timestamp) {
  const qint64 secs =
      timestamp != 0 ? static_cast<qint64>(timestamp) : QDateTime::currentSecsSinceEpoch();
  return QDateTime::fromSecsSinceEpoch(secs).toString("dd/MM/yy, HH:mm");
}

QString privacyLabel(Room::Privacy privacy) {
  return privacy == Room::Privacy::PRIVATE ? QStringLiteral("Private") : QStringLiteral("Public");
}

QString roomListLabel(const Room &room, const QString &currentRoom) {
  const QString name = QString::fromStdString(room.getName());
  QString label = name + QStringLiteral(" room  ·  ") + privacyLabel(room.getPrivacy());
  if (name == currentRoom) {
    label += QStringLiteral("  ·  here");
  }
  return label;
}
} // namespace

// constructor
DashboardPage::DashboardPage(QWidget *parent, Client *client)
    : Page("Distributed Chat", parent, client) {
  connectHeader();
  buildWorkspace();
  getBodyLayout()->setContentsMargins(20, 20, 20, 20);
  refresh();
}

// connect the header
void DashboardPage::connectHeader() {
  header()->signUpButton()->setOnClick([this]() { openSignUpDialog(); });
  header()->logInButton()->setOnClick([this]() { openLogInDialog(); });
  header()->logoutButton()->setOnClick([this]() { logout(); });
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
  auto *sideLayout = new QVBoxLayout(side);
  sideLayout->setContentsMargins(16, 16, 16, 16);
  sideLayout->setSpacing(10);

  // rooms list
  auto *roomsLabel = new QLabel("ROOMS", side);
  roomsLabel->setObjectName("SectionTitle");
  roomsList = new QListWidget(side);

  const bool loggedIn = client() && client()->getState().isLoggedIn();
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
  updateProfileButton = new Button(
      "Update profile", [this]() { openUpdateProfileDialog(); }, true, chat, "GhostButton");
  chatHeader->addWidget(roomTitle, 1);
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
    if (!item) {
      return;
    }
    const QString name = item->data(Qt::UserRole).toString();
    if (name.isEmpty() || name.startsWith('(')) {
      return;
    }
    enterRoom(name);
  });
}

// get the display name
QString DashboardPage::displayName() const {
  if (client() && client()->getState().user) {
    return QString::fromStdString(client()->getState().user->getUsername());
  }
  return QStringLiteral("Guest");
}

// refresh the page
void DashboardPage::refresh() {
  if (!client()) {
    return;
  }

  flushIncomingChat();

  ClientState &state = client()->getState();
  const bool connected = client()->isAlive();
  const QString currentRoom = roomNameOf(state);
  const std::vector<Room> rooms = state.getRooms();

  header()->subtitleLabel()->setText(connected ? "Connected" : "Disconnected");
  header()->setLoggedIn(state.isLoggedIn(), displayName());
  createRoomButton->setVisible(state.isLoggedIn());
  inviteButton->setVisible(state.isLoggedIn());

  // oldest created first (room id is AUTOINCREMENT)
  std::vector<Room> roomsSorted = rooms;
  std::sort(roomsSorted.begin(), roomsSorted.end(),
            [](const Room &a, const Room &b) { return a.getId() < b.getId(); });

  struct RoomRow {
    QString name;
    QString label;
  };
  std::vector<RoomRow> nextRows;
  nextRows.reserve(roomsSorted.size());
  for (const Room &room : roomsSorted) {
    const QString name = QString::fromStdString(room.getName());
    nextRows.push_back({name, roomListLabel(room, currentRoom)});
  }
  if (nextRows.empty()) {
    nextRows.push_back({QString(), connected ? QStringLiteral("(waiting for rooms…)")
                                             : QStringLiteral("(not connected)")});
  }

  bool roomsUnchanged = roomsList->count() == static_cast<int>(nextRows.size());
  if (roomsUnchanged) {
    for (int i = 0; i < static_cast<int>(nextRows.size()); ++i) {
      const QListWidgetItem *item = roomsList->item(i);
      if (!item || item->text() != nextRows[static_cast<std::size_t>(i)].label ||
          item->data(Qt::UserRole).toString() != nextRows[static_cast<std::size_t>(i)].name) {
        roomsUnchanged = false;
        break;
      }
    }
  }

  if (!roomsUnchanged) {
    roomsList->clear();
    for (const RoomRow &row : nextRows) {
      auto *item = new QListWidgetItem(row.label);
      item->setData(Qt::UserRole, row.name);
      roomsList->addItem(item);
    }
  }

  // set the room title
  roomTitle->setText("#  " + currentRoom);
  leaveButton->setEnabled(currentRoom != "Lobby");
}

// append a message to the transcript
void DashboardPage::appendMessage(const QString &author, const QString &text, quint64 timestamp) {
  transcript->appendPlainText(formatChatTime(timestamp) + "  ·  " + author + "  ·  " + text);
}

// drain network chat pushes into the transcript
void DashboardPage::flushIncomingChat() {
  if (!client()) {
    return;
  }
  for (const ChatLine &line : client()->takePendingChatMessages()) {
    appendMessage(QString::fromStdString(line.author), QString::fromStdString(line.text),
                  line.timestamp);
  }
}

// clear transcript and load history for the current room
void DashboardPage::resetTranscriptForCurrentRoom() {
  if (!client() || !transcript) {
    return;
  }
  client()->clearPendingChatMessages();
  transcript->clear();

  std::vector<ChatLine> lines;
  const std::string error = client()->loadMessageHistory(lines);
  if (!error.empty()) {
    appendMessage("system", QString::fromStdString(error));
    return;
  }
  for (const ChatLine &line : lines) {
    appendMessage(QString::fromStdString(line.author), QString::fromStdString(line.text),
                  line.timestamp);
  }
}

// join a room by name (clears transcript, loads history)
bool DashboardPage::enterRoom(const QString &name) {
  if (!client() || name.isEmpty()) {
    return false;
  }

  const std::string error = client()->joinRoom(name.toStdString());
  if (!error.empty()) {
    QMessageBox::warning(this, "Join room", QString::fromStdString(error));
    return false;
  }

  resetTranscriptForCurrentRoom();
  refresh();
  return true;
}

// open the log in dialog
void DashboardPage::openLogInDialog() {
  if (!client()) {
    return;
  }
  if (client()->getState().isLoggedIn()) {
    QMessageBox::information(this, "Log in", "Already logged in.");
    return;
  }

  auto *form = new QWidget;
  auto *layout = new QFormLayout(form);
  auto *userField = new QLineEdit(form);
  auto *passField = new QLineEdit(form);
  passField->setEchoMode(QLineEdit::Password);
  layout->addRow("Username", userField);
  layout->addRow("Password", passField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Log in", form, userField)) {
    return;
  }

  const QString username = userField->text().trimmed();
  const QString password = passField->text();
  if (username.isEmpty()) {
    QMessageBox::warning(this, "Log in", "Username cannot be empty.");
    return;
  }
  if (password.isEmpty()) {
    QMessageBox::warning(this, "Log in", "Password cannot be empty.");
    return;
  }

  const std::string error =
      client()->login(username.toStdString(), password.toStdString());
  if (!error.empty()) {
    QMessageBox::warning(this, "Log in", QString::fromStdString(error));
    return;
  }

  resetTranscriptForCurrentRoom();
  refresh();
}

// open the sign up dialog
void DashboardPage::openSignUpDialog() {
  if (!client()) {
    return;
  }
  if (client()->getState().isLoggedIn()) {
    QMessageBox::information(this, "Sign in", "Already logged in.");
    return;
  }

  auto *form = new QWidget;
  auto *layout = new QFormLayout(form);
  auto *userField = new QLineEdit(form);
  auto *passField = new QLineEdit(form);
  auto *emailField = new QLineEdit(form);
  passField->setEchoMode(QLineEdit::Password);
  layout->addRow("Username", userField);
  layout->addRow("Password", passField);
  layout->addRow("Email", emailField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Sign in", form, userField)) {
    return;
  }

  const QString username = userField->text().trimmed();
  const QString password = passField->text();
  const QString email = emailField->text().trimmed();
  if (username.isEmpty() || email.isEmpty()) {
    QMessageBox::warning(this, "Sign in", "Username and email are required.");
    return;
  }
  if (password.isEmpty()) {
    QMessageBox::warning(this, "Sign in", "Password cannot be empty.");
    return;
  }

  const std::string error =
      client()->signUp(username.toStdString(), password.toStdString(), email.toStdString());
  if (!error.empty()) {
    QMessageBox::warning(this, "Sign in", QString::fromStdString(error));
    return;
  }

  resetTranscriptForCurrentRoom();
  refresh();
}

// open the update profile dialog
void DashboardPage::openUpdateProfileDialog() {
  if (!client() || !client()->getState().isLoggedIn()) {
    QMessageBox::information(this, "Update profile", "Log in first.");
    return;
  }

  const User &user = *client()->getState().user;
  auto *form = new QWidget;
  auto *layout = new QFormLayout(form);
  auto *userField = new QLineEdit(QString::fromStdString(user.getUsername()), form);
  auto *passField = new QLineEdit(form);
  auto *emailField = new QLineEdit(QString::fromStdString(user.getEmail()), form);
  passField->setEchoMode(QLineEdit::Password);
  passField->setPlaceholderText("Leave blank to keep current");
  emailField->setReadOnly(true);
  layout->addRow("Username", userField);
  layout->addRow("New password", passField);
  layout->addRow("Email", emailField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Update profile", form, userField)) {
    return;
  }

  const QString username = userField->text().trimmed();
  const QString password = passField->text();
  if (username.isEmpty()) {
    QMessageBox::warning(this, "Update profile", "Username cannot be empty.");
    return;
  }

  // email is fixed by the server (must match the session profile)
  const std::string error = client()->updateProfile(
      username.toStdString(), password.toStdString(), user.getEmail());
  if (!error.empty()) {
    QMessageBox::warning(this, "Update profile", QString::fromStdString(error));
    return;
  }

  refresh();
}

// open the join room dialog
void DashboardPage::openJoinRoomDialog() {
  if (!client()) {
    return;
  }

  auto *form = new QWidget;
  auto *layout = new QFormLayout(form);
  auto *roomField = new QComboBox(form);
  roomField->setEditable(true);
  for (const Room &room : client()->getState().getRooms()) {
    roomField->addItem(QString::fromStdString(room.getName()));
  }
  layout->addRow("Room", roomField);

  // run the pop up window
  if (!PopUpWindow::run(this, "Join room", form, roomField->lineEdit())) {
    return;
  }

  const QString name = roomField->currentText().trimmed();
  if (name.isEmpty()) {
    QMessageBox::warning(this, "Join room", "Room name cannot be empty.");
    return;
  }

  enterRoom(name);
}

// open the create room dialog
void DashboardPage::openCreateRoomDialog() {
  if (!client() || !client()->getState().isLoggedIn()) {
    QMessageBox::information(this, "Create room", "Log in to create a room.");
    return;
  }

  auto *form = new QWidget;
  auto *layout = new QFormLayout(form);
  auto *nameField = new QLineEdit(form);
  auto *privacy = new QComboBox(form);
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

  const QString privacyText = privacy->currentText();
  const std::string privacyWire =
      (privacyText == QStringLiteral("Private")) ? "PRIVATE" : "PUBLIC";

  const std::string error =
      client()->createRoom(name.toStdString(), privacyWire);
  if (!error.empty()) {
    QMessageBox::warning(this, "Create room", QString::fromStdString(error));
    return;
  }

  // enter the new room (clears transcript + loads history); no "Created" system line
  enterRoom(name);
}

// open the invite dialog
void DashboardPage::openInviteDialog() {
  if (!client()) {
    return;
  }
  if (!client()->getState().isLoggedIn()) {
    QMessageBox::information(this, "Invite", "Log in to invite users.");
    return;
  }

  const QString currentRoom = roomNameOf(client()->getState());
  if (currentRoom == "Lobby") {
    QMessageBox::information(this, "Invite", "Join a private room before inviting.");
    return;
  }

  auto *form = new QWidget;
  auto *layout = new QFormLayout(form);
  auto *invitee = new QLineEdit(form);
  layout->addRow("Room", new QLabel(currentRoom, form));
  layout->addRow("Username", invitee);

  // run the pop up window
  if (!PopUpWindow::run(this, "Invite to room", form, invitee)) {
    return;
  }

  const QString username = invitee->text().trimmed();
  if (username.isEmpty()) {
    QMessageBox::warning(this, "Invite", "Username cannot be empty.");
    return;
  }

  const std::string error =
      client()->inviteToRoom(currentRoom.toStdString(), username.toStdString());
  if (!error.empty()) {
    QMessageBox::warning(this, "Invite", QString::fromStdString(error));
    return;
  }

  QMessageBox::information(this, "Invite", "Invited " + username + " to " + currentRoom + ".");
}

// leave the room
void DashboardPage::leaveRoom() {
  if (!client()) {
    return;
  }

  const std::string error = client()->leaveRoom();
  if (!error.empty()) {
    QMessageBox::warning(this, "Leave room", QString::fromStdString(error));
    return;
  }

  resetTranscriptForCurrentRoom();
  refresh();
}

// send a message
void DashboardPage::sendMessage() {
  if (!client()) {
    return;
  }

  const QString text = composer->text().trimmed();
  if (text.isEmpty()) {
    return;
  }

  const std::string error = client()->sendMessage(text.toStdString());
  if (!error.empty()) {
    QMessageBox::warning(this, "Send message", QString::fromStdString(error));
    return;
  }

  flushIncomingChat();
  composer->clear();
}

// logout
void DashboardPage::logout() {
  if (!client()) {
    return;
  }
  if (!client()->getState().isLoggedIn()) {
    QMessageBox::information(this, "Logout", "Not logged in.");
    return;
  }

  const std::string error = client()->logout();
  if (!error.empty()) {
    QMessageBox::warning(this, "Logout", QString::fromStdString(error));
    return;
  }

  resetTranscriptForCurrentRoom();
  refresh();
}
