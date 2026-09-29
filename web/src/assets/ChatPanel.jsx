function formatChatTime(timestamp) {
  const secs = timestamp ? timestamp : Math.floor(Date.now() / 1000);
  const date = new Date(secs * 1000);
  const pad = (value) => String(value).padStart(2, "0");
  return `${pad(date.getDate())}/${pad(date.getMonth() + 1)}/${String(date.getFullYear()).slice(-2)}, ${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

export default function ChatPanel({ view, draft, onDraftChange, onSend, onUpdateProfile }) {
  return (
    <section className="flex min-w-0 flex-1 flex-col gap-3 rounded-2xl bg-white p-4">
      <div className="flex items-center justify-between gap-3">
        <h2 className="text-lg font-semibold">#  {view.currentRoom}</h2>
        <button
          type="button"
          className="rounded-lg border border-stone-300 px-3 py-2 text-sm"
          disabled={view.busy}
          onClick={onUpdateProfile}
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
          onChange={(event) => onDraftChange(event.target.value)}
        />
        <button type="submit" className="rounded-lg bg-stone-900 px-4 py-2 text-white" disabled={view.busy}>
          Send
        </button>
      </form>
    </section>
  );
}
