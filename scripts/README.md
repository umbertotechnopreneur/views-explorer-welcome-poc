# Scripts

Run every script with `pwsh -NoProfile`.

```powershell
pwsh -NoProfile -File .\scripts\preflight.ps1
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64
pwsh -NoProfile -File .\scripts\validate-signing-boundary.ps1
pwsh -NoProfile -File .\scripts\secret-scan.ps1
```

## Development signing

Signing secrets remain outside the repository under the encrypted workstation
Vault. `manage-signing-certificate.ps1` creates or imports the development
profile without trusting it. `sign-artifacts.ps1` accepts only a public
thumbprint and selects the private key from `CurrentUser\My`; it has no password
parameter.

Both scripts open an interactive menu without arguments and support explicit
actions for tools and automation:

```powershell
pwsh -NoProfile -File .\scripts\manage-signing-certificate.ps1 -Action Status
pwsh -NoProfile -File .\scripts\sign-artifacts.ps1 -Action Verify -Architecture All
```

See [the signing guide](../docs/SIGNING.md) before creating or importing a
certificate.

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
