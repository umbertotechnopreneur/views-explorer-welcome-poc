# Views Explorer Welcome POC agent rules

## Scope

This repository is a public feasibility study for a Windows 11-quality welcome page hosted by File Explorer.

## Boundaries

- `src/ExplorerWelcome.NativeHost` is the lightweight native Win32/C++/WinRT XAML Island experiment. It must stay responsive and avoid heavy orchestration.
- `src/ExplorerWelcome.Broker` is an out-of-process .NET named-pipe broker for slow work and data snapshots.
- `src/ExplorerWelcome.HeavyApp` represents the separate heavier process that consumes the same versioned contract.
- A production Explorer Namespace Extension, COM registration, package identity, and Explorer in-process deployment are not claimed by this first POC.

## Working rules

- Use `pwsh -NoProfile` for every PowerShell command or script launched by an agent.
- Read the root `README.md`, `SECURITY.md`, `IP_PROVENANCE.md`, and `PUBLICATION_CHECKLIST.md` before changing publication-facing files.
- Follow `docs/BRANDING.md` for canonical identity, terminology, claim levels,
  and public wording.
- Keep all source and documentation in English; user discussion may be Italian.
- Preserve unrelated changes and never commit secrets, private paths, generated output, or copied assets with unknown provenance.
- Keep pipe messages versioned, current-user-only, bounded, and explicit about unsupported requests.
- Maintain x64 and ARM64 native configurations. Do not add an architecture fallback that hides a failed native build.
- Run targeted build/smoke checks and update `todo.md`, `lessons.md`, or `archive.md` with evidence.
