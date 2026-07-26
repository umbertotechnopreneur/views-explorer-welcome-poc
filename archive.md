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

## 2026-07-27 — Explorer Home V2 data foundation

- Evolved the pipe protocol to version 2 with correlation IDs, bounded section models, freshness, preferences, and structured action results.
- Added best-effort Windows collectors for machine identity, CPU/memory/network metrics, volumes, network drives, recent items, installed tools, terminal profiles, and documented `ms-settings:` links.
- Added current-user recoverable preferences and safe broker validation for folder and Settings activation.
- Broker + HeavyApp smoke test returned `Machine: WORKSTATION`, `Storage: 5`, `Recent: 5`, and `Tools: 7`.

## 2026-07-27 — Explorer Home V2 visual shell

- Added the shared C++/WinRT XAML visual shell to both the standalone native host and the Namespace Extension view.
- Added wallpaper-aware hero fallback, resource cards, storage cards, network/recent/highlighted/terminal sections, quick Settings links, and bounded button actions.
- Native buttons now send protocol-v2 action requests through the current-user named pipe; no context-menu interfaces were added.
- x64 and ARM64 Release builds passed, followed by the broker + HeavyApp snapshot smoke test.
