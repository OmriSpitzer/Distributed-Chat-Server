function roomLabel(room) {
  const label = `${room.name} room  ·  ${room.privacy}`;
  return room.here ? `${label}  ·  here` : label;
}

export default function RoomSidebar({
  view,
  onJoinRoom,
  onLeave,
  onOpenJoin,
  onOpenCreate,
  onOpenInvite,
  onOpenKick,
  onOpenDelete,
}) {
  const rooms = view.rooms.length
    ? view.rooms
    : [{ id: 0, name: "", privacy: "", here: false, placeholder: view.waiting ? "(waiting for rooms…)" : "(not connected)" }];

  return (
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
                  onJoinRoom(room.name);
                }
              }}
            >
              {room.placeholder ?? roomLabel(room)}
            </button>
          </li>
        ))}
      </ul>
      <div className="grid grid-cols-2 gap-2">
        <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={onOpenJoin}>
          Join room
        </button>
        <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy || view.inLobby} onClick={onLeave}>
          Leave room
        </button>
        {view.loggedIn ? (
          <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={onOpenCreate}>
            Create room
          </button>
        ) : null}
        {view.loggedIn ? (
          <button type="button" className="rounded-lg bg-stone-900 px-2 py-2 text-sm text-white" disabled={view.busy} onClick={onOpenInvite}>
            Invite
          </button>
        ) : null}
        {view.admin ? (
          <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={onOpenKick}>
            Kick
          </button>
        ) : null}
        {view.admin ? (
          <button type="button" className="rounded-lg border border-stone-300 px-2 py-2 text-sm" disabled={view.busy} onClick={onOpenDelete}>
            Delete room
          </button>
        ) : null}
      </div>
    </aside>
  );
}
