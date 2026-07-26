# Lessons

## 2026-07-27 — Keep the first Explorer experiment reversible

- A standalone Win32 host can validate C++/WinRT XAML Island creation and system-theme behavior without registering code inside Explorer.
- The named-pipe broker gives the shell-facing surface a small current-user-only boundary while heavier work stays out of the host.
- A real Explorer Namespace Extension is a separate deployment and lifetime experiment, not an implicit consequence of the standalone host.
- Explorer loads a Namespace Extension as an in-process COM DLL, so the registration scaffold is deliberately per-user, `Apartment` threaded, reversible, and refuses the current host EXE.
- The first real shell boundary can stay small: `IShellFolder`/`IShellView` plus a system-themed XAML Island, with no context-menu interfaces and no heavy data work in Explorer.
- Versioned section models let the native view render partial, stale, or unavailable data without coupling Explorer callbacks to system discovery.

## 2026-07-27 — Public API and lifetime boundaries

- The documented Known Folder catalog does not expose the exact Explorer Quick
  Access pin set, so the product must show that section as unavailable instead
  of reading private destination databases.
- `IShellBrowser::BrowseObject` supports same-browser and new-browser-window
  behavior, but it does not guarantee a new Explorer tab.
- MSIX does not support an in-process Shell Extension loaded into an external
  process such as Explorer. Use a classic installer for the DLL; optional
  external-location package identity applies only to out-of-process components.
- Detached native work must treat XAML Island shutdown as a normal race:
  coalesce refreshes, check a page-lifetime token, and absorb late dispatcher
  failures instead of allowing an exception to terminate Explorer.
- Accessibility must be verified from the UI Automation tree. A visible metric
  label does not automatically give its progress bar an accessible name.
- A post-read message-size check is not a memory bound. Enforce the limit while
  reading the pipe and time out connected clients that do not finish a request.
