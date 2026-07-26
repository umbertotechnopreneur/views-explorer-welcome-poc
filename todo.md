# Task ledger

## Active

- [x] Confirm the native host builds and starts on x64.
- [x] Confirm the native host builds on ARM64.
- [x] Run the broker/client smoke test on the local checkout.
- [x] Decide that the next Explorer experiment is a native COM Namespace Extension; keep packaged `IExplorerCommand` as the context-menu alternative.
- [ ] Prototype a real Explorer-hosted boundary only after the standalone XAML Island host is stable.
- [ ] Implement a minimal native COM Namespace Extension DLL before using `scripts/register-explorer-component.ps1`.
- [ ] Add manifest/MSIX registration after the per-user prototype is proven.

## Completed

- [x] Create the standalone open-source feasibility-study repository.
- [x] Add native host, out-of-process broker, heavy client, and versioned contracts.
- [x] Add publication, security, AI, provenance, and restrictive-license documentation.

## Validation evidence

- `pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64 -Configuration Release` — passed locally.
- Native MSBuild ARM64 release — passed locally.
- Native host process start smoke test — passed locally.
- Broker + HeavyApp named-pipe snapshot — passed locally.
- `pwsh -NoProfile -File .\scripts\secret-scan.ps1` — no likely secret patterns.
- GitHub Actions run `30212826308` — passed managed, native x64, native ARM64, and secret scan.
