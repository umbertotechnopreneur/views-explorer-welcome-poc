<#
.SYNOPSIS
  Run the separate heavy-app contract client.
#>
[CmdletBinding()]
param([switch]$Help)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& dotnet run --project (Join-Path $root 'src\ExplorerWelcome.HeavyApp\ExplorerWelcome.HeavyApp.csproj')
exit $LASTEXITCODE
