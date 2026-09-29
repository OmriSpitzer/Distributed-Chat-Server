import { useEffect, useState } from "react";
import {
  createRoom,
  deleteRoom,
  inviteToRoom,
  joinRoom,
  kickFromRoom,
  leaveRoom,
  login,
  logout,
  sendMessage,
  signUp,
  updateProfile,
  watchState,
} from "./service/chatService.js";

const emptyView = {
  link: "Disconnected",
  busy: false,
  connected: false,
  loggedIn: false,
  admin: false,
  username: "Guest",
  email: "",
  rooms: [],
  waiting: false,
  currentRoom: "Lobby",
  inLobby: true,
  transcript: [],
};

function formatChatTime(timestamp) {
  const secs = timestamp ? timestamp : Math.floor(Date.now() / 1000);
  const date = new Date(secs * 1000);
  const pad = (value) => String(value).padStart(2, "0");
  return `${pad(date.getDate())}/${pad(date.getMonth() + 1)}/${String(date.getFullYear()).slice(-2)}, ${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

function roomLabel(room) {
  const label = `${room.name} room  ·  ${room.privacy}`;
  return room.here ? `${label}  ·  here` : label;
}

function App() {
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

  const rooms = view.rooms.length
    ? view.rooms
    : [{ id: 0, name: "", privacy: "", here: false, placeholder: view.waiting ? "(waiting for rooms…)" : "(not connected)" }];

  const deletable = view.rooms.filter((room) => room.name !== "Lobby" && room.name !== "General");

  return (
    <main className="flex min-h-screen flex-col bg-stone-100 text-stone-900">
      <header className="flex items-center justify-between gap-4 border-b border-stone-200 bg-white px-6 py-4">
        <div>
          <h1 className="text-xl font-semibold">Distributed Chat</h1>
          <p className={view.connected ? "text-sm text-emerald-700" : "text-sm text-rose-700"}>{view.link}</p>
        </div>
        <div className="flex items-center gap-2">
          {view.loggedIn ? (
            <>
              <span className="rounded-full bg-stone-200 px-3 py-1 text-sm">{view.username}</span>
              <button type="button" className="rounded-lg border border-stone-300 px-3 py-2 text-sm" onClick={() => void after("Logout", logout)} disabled={view.busy}>
                Logout
              </button>
            </>
          ) : (
            <>
              <button
                type="button"
                className="rounded-lg border border-stone-300 px-3 py-2 text-sm"
                disabled={view.busy}
                onClick={() => {
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
              >
                Sign in
              </button>
              <button
                type="button"
                className="rounded-lg bg-stone-900 px-3 py-2 text-sm text-white"
                disabled={view.busy}
                onClick={() => {
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
              >
                Log in
              </button>
            </>
          )}
        </div>
      </header>

      <div className="flex min-h-0 flex-1 gap-4 p-5">
        <aside className="flex w-72 shrink-0 flex-col gap-3 rounded-2xl bg-white p-4">
          <p className="text-xs font-semibold tracking-wide text-stone-500">ROOMS</p>
          <ul className="min-h-0 flex-1 space-y-1 overflow-auto">
            {rooms.map((room) => (
              <li key={room.placeholder ?? room.name}>
                <button
                  type="button"
                  className="w-full rounded-lg px-2 py-2 text-left text-sm hover:bg-stone-100"
                  onDoubleClick={() => {
                    if (room.name) {
                      void after("Join room", () => joinRoom(room.name));
                    }
                  }}
                >
                  {room.placeholder ?? roomLabel(room)}
                </button>
              </li>
            ))}
          </ul>
          <div className="grid grid-cols-2 gap-2">
            <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={() => openForm({
              title: "Join room",
              fields: [{ name: "room", label: "Room", options: view.rooms.map((room) => room.name) }],
              submit: submitJoin,
            })}>
              Join room
            </button>
            <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy || view.inLobby} onClick={() => void after("Leave room", leaveRoom)}>
              Leave room
            </button>
            {view.loggedIn ? (
              <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={() => openForm({
                title: "Create room",
                fields: [
                  { name: "name", label: "Name" },
                  { name: "privacy", label: "Privacy", options: ["Public", "Private"] },
                ],
                submit: submitCreate,
              })}>
                Create room
              </button>
            ) : null}
            {view.loggedIn ? (
              <button type="button" className="rounded-lg bg-stone-900 px-2 py-2 text-sm text-white" disabled={view.busy} onClick={() => {
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
              }}>
                Invite
              </button>
            ) : null}
            {view.admin ? (
              <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={() => {
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
              }}>
                Kick
              </button>
            ) : null}
            {view.admin ? (
              <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={() => openForm({
                title: "Delete room",
                fields: [{
                  name: "room",
                  label: "Room",
                  options: deletable.map((room) => room.name),
                  value: deletable.some((room) => room.name === view.currentRoom) ? view.currentRoom : deletable[0]?.name ?? "",
                }],
                submit: submitDelete,
              })}>
                Delete room
              </button>
            ) : null}
          </div>
        </aside>

        <section className="flex min-w-0 flex-1 flex-col gap-3 rounded-2xl bg-white p-4">
          <div className="flex items-center justify-between gap-3">
            <h2 className="text-lg font-semibold">#  {view.currentRoom}</h2>
            <button
              type="button"
              className="rounded-lg border border-stone-300 px-3 py-2 text-sm"
              disabled={view.busy}
              onClick={() => {
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
            >
              Update profile
            </button>
          </div>
          <div className="min-h-0 flex-1 space-y-1 overflow-auto rounded-xl bg-stone-50 p-3 text-sm">
            {view.transcript.map((line, index) => (
              <p key={`${line.timestamp}-${index}`}>
                {formatChatTime(line.timestamp)}  ·  {line.author}  ·  {line.text}
              </p>
            ))}
          </div>
          <form className="flex gap-2" onSubmit={onSend}>
            <input
              className="min-w-0 flex-1 rounded-lg border border-stone-300 px-3 py-2"
              placeholder="Write a message…"
              value={draft}
              onChange={(event) => setDraft(event.target.value)}
            />
            <button type="submit" className="rounded-lg bg-stone-900 px-4 py-2 text-white" disabled={view.busy}>
              Send
            </button>
          </form>
        </section>
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
        <div className="fixed inset-0 z-20 flex items-center justify-center bg-stone-900/40 p-4">
          <section className="w-full max-w-sm rounded-2xl bg-white p-6">
            <h2 className="text-lg font-semibold">{alert.title}</h2>
            <p className="mt-2 text-sm text-stone-600">{alert.message}</p>
            <button type="button" className="mt-4 rounded-lg bg-stone-900 px-4 py-2 text-white" onClick={() => setAlert(null)}>
              OK
            </button>
          </section>
        </div>
      ) : null}
    </main>
  );
}

function FormDialog({ title, fields, onCancel, onSubmit }) {
  const [values, setValues] = useState(() => {
    const initial = {};
    for (const field of fields) {
      initial[field.name] = field.value ?? field.options?.[0] ?? "";
    }
    return initial;
  });

  return (
    <div className="fixed inset-0 z-10 flex items-center justify-center bg-stone-900/40 p-4">
      <form
        className="w-full max-w-sm rounded-2xl bg-white p-6"
        onSubmit={(event) => {
          event.preventDefault();
          onSubmit(values);
        }}
      >
        <h2 className="text-lg font-semibold">{title}</h2>
        <div className="mt-4 space-y-3">
          {fields.map((field) => (
            <label key={field.name} className="block text-sm">
              <span className="mb-1 block text-stone-600">{field.label}</span>
              {field.options ? (
                <>
                  <input
                    className="w-full rounded-lg border border-stone-300 px-3 py-2"
                    list={`${field.name}-options`}
                    value={values[field.name] ?? ""}
                    onChange={(event) => setValues({ ...values, [field.name]: event.target.value })}
                  />
                  <datalist id={`${field.name}-options`}>
                    {field.options.map((option) => (
                      <option key={option} value={option} />
                    ))}
                  </datalist>
                </>
              ) : (
                <input
                  className="w-full rounded-lg border border-stone-300 px-3 py-2"
                  type={field.type ?? "text"}
                  readOnly={field.readOnly}
                  placeholder={field.placeholder}
                  value={values[field.name] ?? ""}
                  onChange={(event) => setValues({ ...values, [field.name]: event.target.value })}
                />
              )}
            </label>
          ))}
        </div>
        <div className="mt-5 flex justify-end gap-2">
          <button type="button" className="rounded-lg border border-stone-300 px-3 py-2 text-sm" onClick={onCancel}>
            Cancel
          </button>
          <button type="submit" className="rounded-lg bg-stone-900 px-3 py-2 text-sm text-white">
            OK
          </button>
        </div>
      </form>
    </div>
  );
}

export default App;
