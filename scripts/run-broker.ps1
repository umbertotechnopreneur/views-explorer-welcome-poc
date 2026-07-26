<#
.SYNOPSIS
  Run the out-of-process broker in the foreground.
#>
[CmdletBinding()]
param([switch]$Help)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& dotnet run --project (Join-Path $root 'src\ExplorerWelcome.Broker\ExplorerWelcome.Broker.csproj')
exit $LASTEXITCODE
