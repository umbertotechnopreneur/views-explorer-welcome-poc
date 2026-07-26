<#
.SYNOPSIS
  Redacted working-tree scan for likely secrets. Reports paths and categories only.
#>
[CmdletBinding()]
param(
    [switch]$Help,
    [string]$Path = (Split-Path -Parent $PSScriptRoot)
)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $Path).Path
$patterns = [ordered]@{
    'private-key' = '-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----'
    'api-key-name' = '(?i)(api[_-]?key|access[_-]?token|client[_-]?secret|password|connection[_-]?string)\s*[:=]'
    'bearer-token' = '(?i)bearer\s+[A-Za-z0-9._-]{16,}'
    'cloud-key' = '(?i)(AKIA[0-9A-Z]{16}|gh[pousr]_[A-Za-z0-9_]{20,})'
}
$excluded = '\\(\.git|bin|obj|node_modules|\.vs)\\'
$findings = [System.Collections.Generic.List[object]]::new()
foreach ($file in Get-ChildItem -LiteralPath $root -Recurse -File -Force -ErrorAction SilentlyContinue) {
    if ($file.FullName -match $excluded -or $file.Length -gt 2MB) { continue }
    try { $lines = Get-Content -LiteralPath $file.FullName -ErrorAction Stop } catch { continue }
    for ($lineNumber = 0; $lineNumber -lt $lines.Count; $lineNumber++) {
        foreach ($entry in $patterns.GetEnumerator()) {
            if ($lines[$lineNumber] -match $entry.Value) {
                $findings.Add([pscustomobject]@{ File = $file.FullName.Substring($root.Length).TrimStart('\\'); Line = $lineNumber + 1; Category = $entry.Key })
            }
        }
    }
}
if ($findings.Count -eq 0) { Write-Host 'No likely secret patterns found in the scanned working tree.'; exit 0 }
$findings | Sort-Object File,Line,Category | Format-Table -AutoSize
Write-Warning 'Review findings without printing or committing secret values.'
exit 2
