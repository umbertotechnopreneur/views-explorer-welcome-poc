# Task ledger

## Active

- [x] Confirm the native host builds and starts on x64.
- [x] Confirm the native host builds on ARM64.
- [x] Run the broker/client smoke test on the local checkout.
- [x] Decide that the next Explorer experiment is a native COM Namespace Extension; do not add context-menu handlers.
- [x] Prototype a real Explorer-hosted boundary after stabilizing the standalone XAML Island host.
- [x] Implement a minimal native COM Namespace Extension DLL before using `scripts/register-explorer-component.ps1`.
- [x] Record the official Explorer Home V2 product and technical specification.
- [x] Evolve the named-pipe contract to explicit Explorer Home V2 section models.
- [x] Add bounded broker collectors for machine identity, metrics, storage, network locations, recent items, tools, terminals, quick settings, and preferences.
- [x] Close the Windows-backed Quick Access/Favorites gate: no proven public
  enumeration contract; keep the section explicitly unavailable.
- [x] Implement the official V2 visual shell in the standalone host and Namespace Extension.
- [x] Add asynchronous snapshot refresh and offline/stale UI states to the native view.
- [x] Add validated folder and settings action requests from the native view; terminal launch remains catalog-only.
- [x] Close the new-Explorer-tab gate: no documented public flag guarantees a
  tab; retain supported same-view and new-window behaviors.
- [x] Implement same-view folder navigation with `IShellBrowser::BrowseObject`.
- [x] Add exact-ID, out-of-process adapters for recent items, Settings, and
  terminal profiles.
- [x] Add negative action-policy tests for command and target injection.
- [x] Close MSIX/sparse-package registration for the in-process Namespace
  Extension as unsupported by the official packaging model.
- [x] Add an identity-only external-location manifest and unsigned build check
  for standalone/out-of-process components.
- [ ] Prototype a classic signed installer with architecture-matched COM and
  Shell namespace registration, rollback, and clean uninstall.

## Completed

- [x] Create the standalone open-source feasibility-study repository.
- [x] Add native host, out-of-process broker, heavy client, and versioned contracts.
- [x] Add publication, security, AI, provenance, and restrictive-license documentation.

## Validation evidence

- `pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64 -Configuration Release` — passed locally.
- Native MSBuild ARM64 release — passed locally.
- Native host process start smoke test — passed locally.
- Namespace Extension DLL x64/ARM64 builds and COM export check — passed locally.
- Namespace Extension register/status/unregister cycle — passed locally; final status is unregistered.
- Broker + HeavyApp named-pipe snapshot — passed locally.
- `pwsh -NoProfile -File .\scripts\secret-scan.ps1` — no likely secret patterns.
- GitHub Actions run `30212826308` — passed managed, native x64, native ARM64, and secret scan.
- Explorer Home V2 specification and single official mockup added under `docs/explorer-home-v2/`.
- V2 broker/HeavyApp smoke test returned machine, storage, recent-item, and tool counts.
