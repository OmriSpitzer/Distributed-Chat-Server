# AGENTS.md

Instructions for any agent (AI or human) working in this repository. Follow them in order for **every** task, no matter how small.

---

## 1. Read all docs first (mandatory)

Before planning or touching code, read **every** Markdown file in the repo. At minimum:

| Doc | Why |
|-----|-----|
| [README.md](README.md) | Overview, features, wire format, build/run, config, layout, CMake targets |
| [architecture.md](architecture.md) | Class layers, Mermaid diagrams, client/server call paths |
| [database.md](database.md) | Per-node SQLite schema, queries, write paths, gossip persistence |
| [tests/TESTS.md](tests/TESTS.md) | Catch2 catalog, tags, totals, untested gaps |
| [FUTURE_WORK.md](FUTURE_WORK.md) | Backlog, goals, what is done vs. open |
| [src/database/sqlite/autosetup/README.md](src/database/sqlite/autosetup/README.md) | Vendored SQLite build notes |

If new `.md` files exist that are not listed here, read them too (search `**/*.md`). Do not skip this step because the task "looks simple" — the docs define conventions the code relies on.

---

## 2. Plan

Before writing code, produce a short plan:

- What is being changed and why (tie it to a `FUTURE_WORK.md` item or the user request).
- Which files/classes are affected (headers in `include/`, implementations in `src/`, tests in `tests/`).
- How it fits the existing architecture (client path, server path, gossip, DB) described in `architecture.md` / `database.md`.
- Which tests will prove it works (new cases and existing ones that may break).

If the task is large or ambiguous, confirm the plan with the user before coding.

---

## 3. Code

- C++17, Windows / Winsock. Match the surrounding style, naming, and comment density.
- Public headers go in `include/<module>/`, implementations in `src/<module>/`.
- New source or test files must be wired into `CMakeLists.txt` (library sources, `*_test` executables, `catch_discover_tests`).
- `DatabaseManager` is the only place that maps SQL rows to domain objects. SQL lives in `src/database/` and is embedded at configure time.
- Gossip payloads use the length-prefixed field format, never `|`-delimited strings.
- Never put password hashes or plaintext on objects meant for UI or the wire.
- Keep changes focused; do not refactor unrelated code.

---

## 4. Test

Every behavior change needs tests.

- Add or update Catch2 cases in the matching `tests/<module>/` file. Use the tags from `TESTS.md`: `[flow]`, `[edge]`, `[thread]` / `[concurrent]`, `[slow]`.
- Build and run the full suite:

```powershell
.\scripts\run_test.ps1
```

- For cluster / gossip changes, also smoke test manually with `.\scripts\run_cluster.ps1` (or `-Test` for console UIs).
- All tests must pass before the task is considered done. Do not delete or weaken tests to make them pass.

---

## 5. Debug

When something fails:

- Read the actual error / CTest output; reproduce with the single failing test executable (e.g. `.\build\client_test.exe "<test name>"`).
- Find the root cause instead of patching symptoms. Check threading, socket lifetime, and SQLite locking first — those are the usual suspects here.
- Remove any temporary debug logging before finishing.
- Re-run the full suite after the fix.

---

## 6. Review

Before handing off, review your own diff (`git diff`):

- No leftover debug code, dead code, or unrelated edits.
- No new compiler warnings or linter errors.
- Docs are in sync:
  - `tests/TESTS.md` — new cases listed, executable/case totals updated.
  - `FUTURE_WORK.md` — completed items checked off, new follow-ups added.
  - `README.md` / `architecture.md` / `database.md` — updated if behavior, packets, schema, or layout changed.

---

## 7. Report (small explanation)

Finish every task with a short summary for the user:

1. **What** was done (one or two sentences).
2. **Files** changed, and why each one.
3. **Tests** added/updated and the result of the full `ctest` run.
4. **Open issues** or follow-ups, if any.

Keep it brief and in plain sentences.
