# Views Explorer Welcome POC

> Windows 11-quality welcome page feasibility study

This repository explores whether a small native Win32/C++/WinRT XAML Island can provide a beautiful, theme-aware welcome surface for File Explorer while heavier functionality remains in a separate process reached through named pipes.

This is a feasibility study, not a production Explorer replacement. The
default workflow keeps Explorer unregistered; the controlled Namespace
Extension experiment is opt-in and loads only the thin shell boundary into
`explorer.exe`.

The repository's Windows CI builds the managed projects and the native host for both x64 and ARM64, then runs a redacted working-tree secret scan.

## Official product specification

The product, interaction, Explorer-integration, data, security, and validation
baseline is documented in
[Explorer Home V2](docs/explorer-home-v2/SPECIFICATION.md). That folder contains
the single approved visual mockup; earlier design explorations are intentionally
excluded.

## Recommended Explorer integration path

For a real new entry under `This PC`, the compatible Shell model is a minimal
native COM Namespace Extension DLL implementing the Shell folder contracts and
keeping every Explorer callback fast. The DLL should be a thin UI boundary;
slow work and the richer dashboard remain in the out-of-process broker and
HeavyApp through the versioned named pipe.

The current XAML host is an executable and is deliberately not registered in
Explorer. Use [scripts/register-explorer-component.ps1](scripts/register-explorer-component.ps1)
only when a real Namespace Extension DLL exists. The script is per-user,
reversible, refuses `.exe` files, and refreshes the Shell association cache.

For production packaging, prefer manifest-based COM registration in an MSIX or
sparse package. The project intentionally does not expose context-menu
handlers; the only target surface is the dedicated `This PC` folder view.

## Architecture

```text
future Explorer Shell / Namespace bridge
        │  lightweight native shell boundary
        ▼
ExplorerWelcome.NamespaceExtension ─── named pipe ─►  ExplorerWelcome.Broker
        │                                             │
        │  COM folder + DesktopWindowXamlSource       │  slow snapshots / orchestration
        │  system XAML controls + Default theme       │
        ▼                                             ▼
     shell-facing surface                     ExplorerWelcome.HeavyApp
                                               separate process client
```

The native host uses the system XAML Island API and leaves the root XAML element on `ElementTheme::Default`, so the Windows theme remains the source of truth. The broker is current-user-only and accepts a versioned JSON-lines protocol. The current shell has the V2 visual sections, validated folder/Settings actions, and an asynchronous snapshot refresh that reports updated, stale, or offline state without blocking Explorer. The heavy client consumes the same contract and is the place for future search, indexing, AI, network, and richer dashboard work.

## Projects

- `ExplorerWelcome.Contracts` — versioned pipe messages and welcome-page models.
- `ExplorerWelcome.Broker` — out-of-process .NET 10 named-pipe server.
- `ExplorerWelcome.HeavyApp` — separate .NET client smoke test and future heavy-app seam.
- `ExplorerWelcome.NativeHost` — native x64/ARM64 Win32 host with `DesktopWindowXamlSource` and a compact themed welcome card.
- `ExplorerWelcome.NamespaceExtension` — native x64/ARM64 COM DLL with an Explorer `IShellFolder`/`IShellView` boundary and no context-menu implementation.

## Build

Open `ExplorerWelcome.slnx` with Visual Studio 2026 line, or use the installed MSBuild path:

```powershell
pwsh -NoProfile -File .\scripts\preflight.ps1
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture ARM64
```

The native project targets Windows SDK `10.0.26100.0` and MSVC `v145` when that toolset is available. The script fails clearly if the requested native toolchain is missing.

## Getting started

Use a normal PowerShell 7 terminal from the repository root. Every command in
this guide deliberately uses `pwsh -NoProfile` so local profiles cannot change
the toolchain or environment.

1. Check the workstation and repository prerequisites:

   ```powershell
   pwsh -NoProfile -File .\scripts\preflight.ps1
   ```

2. Build the complete solution for the architecture that matches the target
   Explorer process:

   ```powershell
   pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64 -Configuration Release
   # or
   pwsh -NoProfile -File .\scripts\build.ps1 -Architecture ARM64 -Configuration Release
   ```

   The Namespace Extension DLL is written to
   `src\ExplorerWelcome.NamespaceExtension\out\<Architecture>\Release\`.

3. Start the out-of-process pieces in separate terminals:

   ```powershell
   pwsh -NoProfile -File .\scripts\run-broker.ps1
   pwsh -NoProfile -File .\scripts\run-heavy-app.ps1
   ```

4. Run the standalone host from Visual Studio or its architecture-specific
   output directory. Use it first to validate XAML rendering, Windows theme
   inheritance, DPI behavior, and the bounded named-pipe ping without touching
   Explorer registration.

## Controlled Explorer Namespace Extension test

The registration script is opt-in and per-user. It refuses the standalone
`.exe`; only the native COM DLL may be registered. Inspect the registry change
without applying it first:

```powershell
pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 `
  -Action Register `
  -ServerPath .\src\ExplorerWelcome.NamespaceExtension\out\x64\Release\ExplorerWelcome.NamespaceExtension.dll `
  -WhatIf
```

If the preview is correct, apply the registration:

```powershell
pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 `
  -Action Register `
  -ServerPath .\src\ExplorerWelcome.NamespaceExtension\out\x64\Release\ExplorerWelcome.NamespaceExtension.dll `
  -Confirm:$false
```

Open `This PC` in File Explorer. If the entry is not visible, close and reopen
the Explorer window; the script already sends the Shell association-change
notification. Verify the registration at any time:

```powershell
pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 -Action Status
```

Remove the experiment when finished:

```powershell
pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 -Action Unregister -Confirm:$false
```

The script also opens an interactive menu when invoked without parameters. The
default CLSID belongs to this POC; pass `-Clsid` only when testing another
reviewed component. Do not use `-Force` unless the existing registration path
has been inspected.

## Development and validation loop

- Keep `IShellFolder`/`IShellView` callbacks fast and deterministic. No network,
  indexing, AI, shell commands, or long filesystem scans belong in Explorer.
- Put slow work in `ExplorerWelcome.Broker` or `ExplorerWelcome.HeavyApp` and
  extend the versioned named-pipe contract instead of adding it to the DLL.
- Build both architectures before changing the registration target.
- Run the local checks before committing:

  ```powershell
  pwsh -NoProfile -File .\scripts\preflight.ps1
  pwsh -NoProfile -File .\scripts\secret-scan.ps1
  pwsh -NoProfile -Command "gitleaks detect --source . --redact --no-banner"
  pwsh -NoProfile -Command "git diff --check"
  ```

- Inspect the COM boundary when needed:

  ```powershell
  pwsh -NoProfile -Command "& 'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\dumpbin.exe' /exports '.\src\ExplorerWelcome.NamespaceExtension\out\x64\Release\ExplorerWelcome.NamespaceExtension.dll'"
  ```

The live Explorer test remains a feasibility test: a fault in an in-process
Shell DLL can affect Explorer. Keep registration reversible and leave the
component unregistered when it is not actively being tested.

## Run the smoke test

Terminal 1:

```powershell
pwsh -NoProfile -File .\scripts\run-broker.ps1
```

Terminal 2:

```powershell
pwsh -NoProfile -File .\scripts\run-heavy-app.ps1
```

Then launch the native host from Visual Studio or the architecture-specific output folder. The native card can send bounded `host.ping`, snapshot, folder, and documented Settings requests to the broker; snapshot collection and action launching stay outside the shell-facing path. The same card is hosted by the Namespace Extension when the controlled Explorer test is registered.

## What this POC does not prove yet

- Safe lifetime behavior inside a real Explorer process.
- Package identity, MSIX deployment, or manifest-based COM registration.
- Production accessibility, signing, telemetry, search/indexing, or enterprise servicing.

Those are explicit next experiments after the host and pipe contract are stable.

## License

Source code is released under the [PolyForm Noncommercial License 1.0.0](LICENSE). Commercial use, commercial distribution, or embedding in a commercial product requires separate written authorization from the copyright holder. Third-party components and assets remain under their own licenses.

## AI assistance

AI tools may assist with drafts, implementation, tests, and documentation. A human contributor remains responsible for review, provenance, security, licensing, validation, and publication decisions. See [AI_CONTRIBUTION_POLICY.md](AI_CONTRIBUTION_POLICY.md).
