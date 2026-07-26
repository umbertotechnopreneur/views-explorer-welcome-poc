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

## 2026-07-27 — Explorer Home V2 async refresh boundary

- Added a non-blocking snapshot refresh callback shared by the standalone host and Namespace Extension.
- Broker work runs outside the XAML/Explorer callback thread and reports updated, stale, or offline state back through the XAML dispatcher.
- Native refresh requests use protocol-v2 correlation IDs and preserve cached visual content when the broker is unavailable.

## 2026-07-27 — Explorer Home V2 live dashboard

- Bound the native XAML view to real broker snapshots for machine identity,
  wallpaper, metrics, volumes, network locations, recent items, terminals, and
  Settings.
- Added a bounded current-user snapshot cache with stale/offline rendering.
- Confirmed the dashboard visually with the native host; x64 and ARM64 Debug
  builds passed with no native warnings or errors.
- Unknown disk health is labelled `Stato sconosciuto`, never healthy.

## 2026-07-27 — Supported navigation and action boundary

- Added same-view folder navigation through
  `IShellBrowser::BrowseObject(SBSP_SAMEBROWSER)`.
- Added out-of-process, exact-ID adapters for recent items, documented Settings
  targets, folders, and terminal profiles; PowerShell always starts with
  `-NoProfile`.
- Closed the Quick Access pin and guaranteed-new-tab gates as unavailable under
  the currently documented public Windows contracts.
- Ten broker action-policy tests pass, including command and target injection
  rejection cases.

## 2026-07-27 — Packaging feasibility gate

- Closed MSIX and sparse-package COM registration for the Namespace Extension:
  official MSIX guidance excludes in-process Shell extensions loaded by an
  external process such as Explorer.
- Recorded the supported hybrid direction: classic installer for the native
  Explorer DLL, with optional external-location identity only for standalone
  and out-of-process components.
- Added an unsigned identity-only manifest and bounded build script; it never
  signs, trusts, installs, registers, or removes a package.

## 2026-07-27 — First Explorer safety hardening pass

- Added page-lifetime and refresh-coalescing guards so late broker completions
  cannot update a closing XAML Island or propagate dispatcher failure.
- Limited concurrent detached action requests and kept all activation outside
  Explorer.
- Expanded the broker suite to 26 passing tests covering bounded physical
  framing, malformed JSON, protocol versions, depth, size, correlation IDs,
  control characters, arguments, and action injection.
- Added a five-second connected-client read timeout so an idle local pipe
  client cannot monopolize the single broker endpoint.
- Added accessible names for metric and storage progress bars plus a polite
  live dashboard status.
- The hidden native host stress mode passed 50 XAML create/teardown cycles.
