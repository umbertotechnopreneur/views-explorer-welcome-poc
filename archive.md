# Archive

Completed work is recorded here with date and validation evidence. Open work stays in `todo.md`.

## 2026-07-27 — Initial feasibility repository

- Created and pushed `umbertotechnopreneur/views-explorer-welcome-poc`.
- Added the system-themed C++/WinRT XAML Island host, .NET broker, HeavyApp client, versioned current-user-only named pipe, restrictive noncommercial license, and open-source governance files.
- Local x64/ARM64 builds and named-pipe smoke test passed. GitHub Actions run `30212826308` passed both native architectures and the redacted secret scan.

## 2026-07-27 — Native Namespace Extension boundary

- Added `ExplorerWelcome.NamespaceExtension`, a native COM DLL implementing the minimal `IShellFolder`/`IShellView` boundary with a system-themed XAML Island.
- Explicitly excluded context-menu interfaces; slow work remains in the broker and HeavyApp.
- x64/ARM64 builds, COM export inspection, and a per-user register/status/unregister cycle passed locally.
