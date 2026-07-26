# ExplorerWelcome.NamespaceExtension

This project is the first real Explorer boundary in the feasibility study. It
is a native in-process COM DLL that exposes a minimal Shell Namespace Extension
root for the `This PC` namespace.

The DLL deliberately implements no context-menu handler. Its Shell-facing
contract is limited to the folder object, a custom `IShellView`, and a small
Win32/C++/WinRT XAML Island. The view renders the same system-themed welcome
card as the standalone host; the broker and HeavyApp remain out of process.

The DLL is not registered by the build. Use the repository script only after
reviewing the output path:

```powershell
pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 `
  -Action Register `
  -ServerPath .\src\ExplorerWelcome.NamespaceExtension\out\x64\Release\ExplorerWelcome.NamespaceExtension.dll `
  -WhatIf
```

Registration is per-user and reversible. Explorer loads this DLL inside its
process, so this remains a controlled feasibility experiment rather than a
production deployment mechanism.
