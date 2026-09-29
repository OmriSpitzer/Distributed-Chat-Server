/**
 * App component
 * @brief Composes the chat dashboard and owns its session state.
 * @date 2026-09-29
 *
 * Panels live in assets/. Services own the socket and every request.
 */

import { useEffect, useState } from "react";

// Chat components
import AlertDialog from "./assets/AlertDialog.jsx";
import ChatPanel from "./assets/ChatPanel.jsx";
import FormDialog from "./assets/FormDialog.jsx";
import Header from "./assets/Header.jsx";
import RoomSidebar from "./assets/RoomSidebar.jsx";

// Chat services
import { kickFromRoom, deleteRoom } from "./service/adminService.js";
import { watchState } from "./service/chatService.js";
import { createRoom, inviteToRoom, joinRoom, leaveRoom, sendMessage } from "./service/roomService.js";
import { login, logout, signUp, updateProfile } from "./service/userService.js";

// Empty view state
const emptyView = {
  link: "Disconnected", busy: false, connected: false, loggedIn: false, admin: false,
  username: "Guest", email: "", rooms: [], waiting: false, currentRoom: "Lobby",
  inLobby: true, transcript: [],
};

// Main App component
const App = () => {
  const [view, setView] = useState(emptyView);
  const [dialog, setDialog] = useState(null);
  const [alert, setAlert] = useState(null);
  const [draft, setDraft] = useState("");

  useEffect(() => watchState(setView), []);

  function openForm(next) {
    setDialog(next);
  }

  async function after(title, task, success) {
    setDialog(null);
    const error = await task();
    if (error === "busy") {
      return;
    }
    if (error) {
      setAlert({ title, message: error });
      return;
    }
    if (success) {
      setAlert({ title, message: success });
    }
  }

  function submitLogin(fields) {
    const username = fields.username.trim();
    const password = fields.password;
    if (!username) {
      return "Username cannot be empty.";
    }
    if (!password) {
      return "Password cannot be empty.";
    }
    void after("Log in", () => login(username, password));
    return "";
  }

  function submitSignUp(fields) {
    const username = fields.username.trim();
    const password = fields.password;
    const email = fields.email.trim();
    if (!username || !email) {
      return "Username and email are required.";
    }
    if (!password) {
      return "Password cannot be empty.";
    }
    void after("Sign in", () => signUp(username, password, email));
    return "";
  }

  function submitProfile(fields) {
    const username = fields.username.trim();
    if (!username) {
      return "Username cannot be empty.";
    }
    void after("Update profile", () => updateProfile(username, fields.password, view.email));
    return "";
  }

  function submitJoin(fields) {
    const name = fields.room.trim();
    if (!name) {
      return "Room name cannot be empty.";
    }
    void after("Join room", () => joinRoom(name));
    return "";
  }

  function submitCreate(fields) {
    const name = fields.name.trim();
    if (!name) {
      return "Room name cannot be empty.";
    }
    const privacy = fields.privacy === "Private" ? "PRIVATE" : "PUBLIC";
    void after("Create room", async () => {
      const error = await createRoom(name, privacy);
      if (error) {
        return error;
      }
      return joinRoom(name);
    });
    return "";
  }

  function submitInvite(fields) {
    const username = fields.username.trim();
    if (!username) {
      return "Username cannot be empty.";
    }
    void after("Invite", () => inviteToRoom(username), `Invited ${username} to ${view.currentRoom}.`);
    return "";
  }

  function submitKick(fields) {
    const username = fields.username.trim();
    if (!username) {
      return "Username cannot be empty.";
    }
    void after("Kick", () => kickFromRoom(username), `Kicked ${username} from ${view.currentRoom}.`);
    return "";
  }

  function submitDelete(fields) {
    const name = fields.room.trim();
    if (!name) {
      return "Room name cannot be empty.";
    }
    if (name === "Lobby" || name === "General") {
      return "Cannot delete Lobby or General.";
    }
    void after("Delete room", () => deleteRoom(name));
    return "";
  }

  async function onSend(event) {
    event.preventDefault();
    const text = draft.trim();
    if (!text) {
      return;
    }
    const error = await sendMessage(text);
    if (error === "busy") {
      return;
    }
    if (error) {
      setAlert({ title: "Send message", message: error });
      return;
    }
    setDraft("");
  }

  const deletable = view.rooms.filter((room) => room.name !== "Lobby" && room.name !== "General");

  return (
    <main className="flex min-h-screen flex-col bg-stone-100 text-stone-900">
      <Header
        view={view}
        onLogout={() => void after("Logout", logout)}
        onSignIn={() => {
          if (view.loggedIn) {
            setAlert({ title: "Sign in", message: "Already logged in." });
            return;
          }
          openForm({
            title: "Sign in",
            fields: [
              { name: "username", label: "Username" },
              { name: "password", label: "Password", type: "password" },
              { name: "email", label: "Email" },
            ],
            submit: submitSignUp,
          });
        }}
        onLogIn={() => {
          if (view.loggedIn) {
            setAlert({ title: "Log in", message: "Already logged in." });
            return;
          }
          openForm({
            title: "Log in",
            fields: [
              { name: "username", label: "Username" },
              { name: "password", label: "Password", type: "password" },
            ],
            submit: submitLogin,
          });
        }}
      />

      <div className="flex min-h-0 flex-1 gap-4 p-5">
        <RoomSidebar
          view={view}
          onJoinRoom={(name) => void after("Join room", () => joinRoom(name))}
          onLeave={() => void after("Leave room", leaveRoom)}
          onOpenJoin={() => openForm({
            title: "Join room",
            fields: [{ name: "room", label: "Room", options: view.rooms.map((room) => room.name) }],
            submit: submitJoin,
          })}
          onOpenCreate={() => openForm({
            title: "Create room",
            fields: [
              { name: "name", label: "Name" },
              { name: "privacy", label: "Privacy", options: ["Public", "Private"] },
            ],
            submit: submitCreate,
          })}
          onOpenInvite={() => {
            if (view.inLobby) {
              setAlert({ title: "Invite", message: "Join a room before inviting." });
              return;
            }
            openForm({
              title: "Invite to room",
              fields: [
                { name: "roomLabel", label: "Room", value: view.currentRoom, readOnly: true },
                { name: "username", label: "Username" },
              ],
              submit: submitInvite,
            });
          }}
          onOpenKick={() => {
            if (view.inLobby) {
              setAlert({ title: "Kick", message: "Join a room before kicking." });
              return;
            }
            openForm({
              title: "Kick from room",
              fields: [
                { name: "roomLabel", label: "Room", value: view.currentRoom, readOnly: true },
                { name: "username", label: "Username" },
              ],
              submit: submitKick,
            });
          }}
          onOpenDelete={() => openForm({
            title: "Delete room",
            fields: [{
              name: "room",
              label: "Room",
              options: deletable.map((room) => room.name),
              value: deletable.some((room) => room.name === view.currentRoom) ? view.currentRoom : deletable[0]?.name ?? "",
            }],
            submit: submitDelete,
          })}
        />
        <ChatPanel
          view={view}
          draft={draft}
          onDraftChange={setDraft}
          onSend={onSend}
          onUpdateProfile={() => {
            if (!view.loggedIn) {
              setAlert({ title: "Update profile", message: "Log in first." });
              return;
            }
            openForm({
              title: "Update profile",
              fields: [
                { name: "username", label: "Username", value: view.username },
                { name: "password", label: "New password", type: "password", placeholder: "Leave blank to keep current" },
                { name: "email", label: "Email", value: view.email, readOnly: true },
              ],
              submit: submitProfile,
            });
          }}
        />
      </div>

      {dialog ? (
        <FormDialog
          title={dialog.title}
          fields={dialog.fields}
          onCancel={() => setDialog(null)}
          onSubmit={(fields) => {
            const error = dialog.submit(fields);
            if (error) {
              setAlert({ title: dialog.title, message: error });
            }
          }}
        />
      ) : null}
      {alert ? (
        <AlertDialog title={alert.title} message={alert.message} onClose={() => setAlert(null)} />
      ) : null}
    </main>
  );
};

export default App;
