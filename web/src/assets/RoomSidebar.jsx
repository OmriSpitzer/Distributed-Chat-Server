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
    <aside className="side">
      <p className="section">ROOMS</p>
      <ul className="room-list">
        {rooms.map((room) => (
          <li key={room.placeholder ?? room.name}>
            <button
              type="button"
              className="room-button"
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
      <div className="actions">
        <button type="button" className="ghost" disabled={view.busy} onClick={onOpenJoin}>
          Join room
        </button>
        <button type="button" className="ghost" disabled={view.busy || view.inLobby} onClick={onLeave}>
          Leave room
        </button>
        {view.loggedIn ? (
          <button type="button" className="ghost" disabled={view.busy} onClick={onOpenCreate}>
            Create room
          </button>
        ) : null}
        {view.loggedIn ? (
          <button type="button" className="primary" disabled={view.busy} onClick={onOpenInvite}>
            Invite
          </button>
        ) : null}
        {view.admin ? (
          <button type="button" className="ghost" disabled={view.busy} onClick={onOpenKick}>
            Kick
          </button>
        ) : null}
        {view.admin ? (
          <button type="button" className="ghost" disabled={view.busy} onClick={onOpenDelete}>
            Delete room
          </button>
        ) : null}
      </div>
    </aside>
  );
}
