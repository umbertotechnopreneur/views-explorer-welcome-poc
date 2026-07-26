# Lessons

## 2026-07-27 — Keep the first Explorer experiment reversible

- A standalone Win32 host can validate C++/WinRT XAML Island creation and system-theme behavior without registering code inside Explorer.
- The named-pipe broker gives the shell-facing surface a small current-user-only boundary while heavier work stays out of the host.
- A real Explorer Namespace Extension is a separate deployment and lifetime experiment, not an implicit consequence of the standalone host.
- Explorer loads a Namespace Extension as an in-process COM DLL, so the registration scaffold is deliberately per-user, `Apartment` threaded, reversible, and refuses the current host EXE.
- A packaged `IExplorerCommand` is the safer supported route for a context-menu action, but it cannot provide the custom `This PC` dashboard surface we are studying.
