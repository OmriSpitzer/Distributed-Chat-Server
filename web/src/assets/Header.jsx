import { useUi } from "../contexts/UiContext.jsx";

export default function Header({ view, onLogout, onSignIn, onLogIn }) {
  const { toggleTheme, toggleDensity } = useUi();

  return (
    <header className="header">
      <div>
        <h1 className="title">Distributed Chat</h1>
        <p className={view.connected ? "ok" : "bad"}>{view.link}</p>
      </div>
      <div className="header-actions">
        <button type="button" className="ghost" onClick={toggleTheme}>
          <span className="label-light">Light</span>
          <span className="label-dark">Dark</span>
        </button>
        <button type="button" className="ghost" onClick={toggleDensity}>
          <span className="label-comfortable">Comfortable</span>
          <span className="label-compact">Compact</span>
        </button>
        {view.loggedIn ? (
          <>
            <span className="chip">{view.username}</span>
            <button type="button" className="ghost" onClick={onLogout} disabled={view.busy}>
              Logout
            </button>
          </>
        ) : (
          <>
            <button type="button" className="ghost" disabled={view.busy} onClick={onSignIn}>
              Sign in
            </button>
            <button type="button" className="primary" disabled={view.busy} onClick={onLogIn}>
              Log in
            </button>
          </>
        )}
      </div>
    </header>
  );
}
