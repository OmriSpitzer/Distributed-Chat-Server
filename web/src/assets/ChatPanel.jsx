import { useLayoutEffect, useRef, useState } from "react";

function formatChatTime(timestamp) {
  const secs = timestamp ? timestamp : Math.floor(Date.now() / 1000);
  const date = new Date(secs * 1000);
  const pad = (value) => String(value).padStart(2, "0");
  return `${pad(date.getDate())}/${pad(date.getMonth() + 1)}/${String(date.getFullYear()).slice(-2)}, ${pad(date.getHours())}:${pad(date.getMinutes())}`;
}

function sameLine(left, right) {
  return left.author === right.author && left.text === right.text && left.timestamp === right.timestamp;
}

function extendsTranscript(previous, next) {
  if (next.length < previous.length) {
    return false;
  }
  return previous.every((line, index) => sameLine(line, next[index]));
}

function atBottom(element) {
  return element.scrollHeight - element.scrollTop - element.clientHeight <= 2;
}

export default function ChatPanel({ view, draft, onDraftChange, onSend, onUpdateProfile }) {
  const transcriptRef = useRef(null);
  const pinnedRef = useRef(true);
  const previousRef = useRef([]);
  const [unread, setUnread] = useState(0);

  useLayoutEffect(() => {
    const element = transcriptRef.current;
    const previous = previousRef.current;
    const next = view.transcript;
    const pinned = pinnedRef.current;
    const extended = extendsTranscript(previous, next);
    previousRef.current = next;
    if (!element) {
      return;
    }
    const added = next.length - previous.length;
    if (!extended || pinned) {
      element.scrollTop = element.scrollHeight;
      pinnedRef.current = true;
      setUnread(0);
      return;
    }
    if (added > 0) {
      setUnread((count) => count + added);
    }
  }, [view.transcript]);

  function onTranscriptScroll() {
    const element = transcriptRef.current;
    if (!element) {
      return;
    }
    const pinned = atBottom(element);
    pinnedRef.current = pinned;
    if (pinned) {
      setUnread(0);
    }
  }

  function scrollToBottom() {
    const element = transcriptRef.current;
    if (element) {
      element.scrollTop = element.scrollHeight;
    }
    pinnedRef.current = true;
    setUnread(0);
  }

  function handleSend(event) {
    pinnedRef.current = true;
    onSend(event);
  }

  const unreadLabel = unread === 1 ? "1 new" : `${unread} new`;

  return (
    <section className="chat">
      <div className="chat-head">
        <h2 className="room-title">#  {view.currentRoom}</h2>
        <button type="button" className="ghost" disabled={view.busy} onClick={onUpdateProfile}>
          Update profile
        </button>
      </div>
      <div className="transcript" ref={transcriptRef} onScroll={onTranscriptScroll}>
        {view.transcript.map((line, index) => (
          <p key={`${line.timestamp}-${index}`}>
            {formatChatTime(line.timestamp)}  ·  {line.author}  ·  {line.text}
          </p>
        ))}
      </div>
      <form className="composer" onSubmit={handleSend}>
        {unread > 0 ? (
          <button type="button" className="primary" onClick={scrollToBottom}>
            {unreadLabel}
          </button>
        ) : null}
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
