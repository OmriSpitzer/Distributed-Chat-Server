export default function AlertDialog({ title, message, onClose }) {
  return (
    <div className="fixed inset-0 z-20 flex items-center justify-center bg-stone-900/40 p-4">
      <section className="w-full max-w-sm rounded-2xl bg-white p-6">
        <h2 className="text-lg font-semibold">{title}</h2>
        <p className="mt-2 text-sm text-stone-600">{message}</p>
        <button type="button" className="mt-4 rounded-lg bg-stone-900 px-4 py-2 text-white" onClick={onClose}>
          OK
        </button>
      </section>
    </div>
  );
}
