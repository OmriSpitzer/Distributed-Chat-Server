function formatChatTime(timestamp) {
  const secs = timestamp ? timestamp : Math.floor(Date.now() / 1000);
  const date = new Date(secs * 1000);
  const pad = (value) => String(value).padStart(2, "0");
  return `${pad(date.getDate())}/${pad(date.getMonth() + 1)}/${String(date.getFullYear()).slice(-2)}, ${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

export default function ChatPanel({ view, draft, onDraftChange, onSend, onUpdateProfile }) {
  return (
    <section className="chat">
      <div className="chat-head">
        <h2 className="room-title">#  {view.currentRoom}</h2>
        <button type="button" className="ghost" disabled={view.busy} onClick={onUpdateProfile}>
          Update profile
        </button>
      </div>
      <div className="transcript">
        {view.transcript.map((line, index) => (
          <p key={`${line.timestamp}-${index}`}>
            {formatChatTime(line.timestamp)}  ·  {line.author}  ·  {line.text}
          </p>
        ))}
      </div>
      <form className="composer" onSubmit={onSend}>
        <input
          className="field"
          placeholder="Write a message…"
          value={draft}
          onChange={(event) => onDraftChange(event.target.value)}
        />
        <button type="submit" className="primary" disabled={view.busy}>
          Send
        </button>
      </form>
    </section>
  );
}
