export default function AlertDialog({ title, message, onClose }) {
  return (
    <div className="scrim scrim-top">
      <section className="dialog">
        <h2 className="room-title">{title}</h2>
        <p className="alert-copy">{message}</p>
        <button type="button" className="primary alert-ok" onClick={onClose}>
          OK
        </button>
      </section>
    </div>
  );
}
