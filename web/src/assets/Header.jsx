export default function Header({ view, onLogout, onSignIn, onLogIn }) {
  return (
    <header className="flex items-center justify-between gap-4 border-b border-stone-200 bg-white px-6 py-4">
      <div>
        <h1 className="text-xl font-semibold">Distributed Chat</h1>
        <p className={view.connected ? "text-sm text-emerald-700" : "text-sm text-rose-700"}>{view.link}</p>
      </div>
      <div className="flex items-center gap-2">
        {view.loggedIn ? (
          <>
            <span className="rounded-full bg-stone-200 px-3 py-1 text-sm">{view.username}</span>
            <button type="button" className="rounded-lg border border-stone-300 px-3 py-2 text-sm" onClick={onLogout} disabled={view.busy}>
              Logout
            </button>
          </>
        ) : (
          <>
            <button type="button" className="rounded-lg border border-stone-300 px-3 py-2 text-sm" disabled={view.busy} onClick={onSignIn}>
              Sign in
            </button>
            <button type="button" className="rounded-lg bg-stone-900 px-3 py-2 text-sm text-white" disabled={view.busy} onClick={onLogIn}>
              Log in
            </button>
          </>
        )}
      </div>
    </header>
  );
}
