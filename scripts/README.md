# Scripts

Run every script with `pwsh -NoProfile`.

```powershell
pwsh -NoProfile -File .\scripts\preflight.ps1
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64
pwsh -NoProfile -File .\scripts\secret-scan.ps1
```
