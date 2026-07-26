---
name: Explorer Welcome POC
description: Rules for the native XAML Island feasibility study and process-isolated broker.
alwaysApply: true
---

# Repository rules

- Read `AGENTS.md` before editing.
- This is an open-source noncommercial feasibility study, not production Explorer code.
- Keep `ExplorerWelcome.NativeHost` lightweight, responsive, and free of slow orchestration.
- Keep heavy work in `ExplorerWelcome.Broker` or `ExplorerWelcome.HeavyApp` behind the versioned current-user-only pipe contract.
- Use `pwsh -NoProfile` for every PowerShell command.
- Build both `x64` and `ARM64` where the native project applies.
- Preserve provenance, security, license, and publication files.
