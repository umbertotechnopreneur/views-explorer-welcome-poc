# Scripts

Run every script with `pwsh -NoProfile`.

```powershell
pwsh -NoProfile -File .\scripts\preflight.ps1
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64
pwsh -NoProfile -File .\scripts\secret-scan.ps1
```

## Explorer registration prototype

`register-explorer-component.ps1` is a per-user, reversible registration
scaffold for the next native COM Namespace Extension experiment. It writes
only under `HKCU`, requires a real `.dll`, sets `ThreadingModel=Apartment`,
and refuses the current XAML host `.exe`.

Run it without parameters to open its interactive menu. Supplying
`-Action`, `-Clsid`, and (for registration) `-ServerPath` makes it suitable for
automation and tool invocation.

Always inspect the planned change first:

```powershell
pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 `
  -Action Register `
  -Clsid '{a714cffa-a7b2-49fd-9f15-e42b1aefbca5}' `
  -ServerPath '.\src\ExplorerWelcome.NamespaceExtension\out\x64\Release\ExplorerWelcome.NamespaceExtension.dll' `
  -WhatIf
```

Use `-Action Status` to inspect one CLSID and `-Action Unregister` to remove
the matching per-user registration. Do not register the current standalone
`ExplorerWelcome.NativeHost.exe`; it is not a COM in-process server.
