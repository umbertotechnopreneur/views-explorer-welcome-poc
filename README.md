# Views Explorer Welcome POC

> Windows 11-quality welcome page feasibility study

This repository explores whether a small native Win32/C++/WinRT XAML Island can provide a beautiful, theme-aware welcome surface for File Explorer while heavier functionality remains in a separate process reached through named pipes.

This is a feasibility study, not a production Explorer replacement. The current proof validates the pieces that can be tested safely without modifying Explorer registration or loading application logic into `explorer.exe`.

## Architecture

```text
future Explorer Shell / Namespace bridge
        │  lightweight native host boundary
        ▼
ExplorerWelcome.NativeHost  ───── named pipe ─────►  ExplorerWelcome.Broker
        │                                             │
        │  Win32 + DesktopWindowXamlSource            │  slow snapshots / orchestration
        │  system XAML controls + Default theme       │
        ▼                                             ▼
     shell-facing surface                     ExplorerWelcome.HeavyApp
                                               separate process client
```

The native host uses the system XAML Island API and leaves the root XAML element on `ElementTheme::Default`, so the Windows theme remains the source of truth. The broker is current-user-only and accepts a versioned JSON-lines protocol. The heavy client consumes the same contract and is the place for future search, indexing, AI, network, and richer dashboard work.

## Projects

- `ExplorerWelcome.Contracts` — versioned pipe messages and welcome-page models.
- `ExplorerWelcome.Broker` — out-of-process .NET 10 named-pipe server.
- `ExplorerWelcome.HeavyApp` — separate .NET client smoke test and future heavy-app seam.
- `ExplorerWelcome.NativeHost` — native x64/ARM64 Win32 host with `DesktopWindowXamlSource` and a compact themed welcome card.

## Build

Open `ExplorerWelcome.slnx` with Visual Studio 2026 line, or use the installed MSBuild path:

```powershell
pwsh -NoProfile -File .\scripts\preflight.ps1
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture ARM64
```

The native project targets Windows SDK `10.0.26100.0` and MSVC `v145` when that toolset is available. The script fails clearly if the requested native toolchain is missing.

## Run the smoke test

Terminal 1:

```powershell
pwsh -NoProfile -File .\scripts\run-broker.ps1
```

Terminal 2:

```powershell
pwsh -NoProfile -File .\scripts\run-heavy-app.ps1
```

Then launch the native host from Visual Studio or the architecture-specific output folder. The native card can send a bounded `host.ping` to the broker; it does not perform heavy data work on the shell-facing path.

## What this POC does not prove yet

- Explorer Namespace Extension registration and deployment.
- Safe lifetime behavior inside a real Explorer process.
- Package identity, MSIX deployment, or Windows App SDK bootstrap policy.
- Production accessibility, signing, telemetry, search/indexing, or enterprise servicing.

Those are explicit next experiments after the host and pipe contract are stable.

## License

Source code is released under the [PolyForm Noncommercial License 1.0.0](LICENSE). Commercial use, commercial distribution, or embedding in a commercial product requires separate written authorization from the copyright holder. Third-party components and assets remain under their own licenses.

## AI assistance

AI tools may assist with drafts, implementation, tests, and documentation. A human contributor remains responsible for review, provenance, security, licensing, validation, and publication decisions. See [AI_CONTRIBUTION_POLICY.md](AI_CONTRIBUTION_POLICY.md).
