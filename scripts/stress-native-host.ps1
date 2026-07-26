<#
===============================================================================
Views Explorer Welcome POC
File: scripts/stress-native-host.ps1
Purpose: Run the hidden 50-cycle XAML page construction and teardown smoke test.

Copyright (c) 2026 Umberto Giacobbi
Author: Umberto Giacobbi
Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
License: PolyForm Noncommercial License 1.0.0
SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
Open source: https://umbertogiacobbi.biz/opensource
===============================================================================
#>

<#
.SYNOPSIS
  Run the hidden 50-cycle XAML page construction and teardown smoke test.
#>
[CmdletBinding()]
param(
    [switch]$Help,
    [ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug'
)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$hostPath = Join-Path $root (
    "src\ExplorerWelcome.NativeHost\out\{0}\{1}\ExplorerWelcome.NativeHost.exe" -f
        $Architecture,
        $Configuration)

if (-not (Test-Path -LiteralPath $hostPath -PathType Leaf)) {
    throw "Native host not found: $hostPath. Build the requested architecture first."
}

$process = Start-Process `
    -FilePath $hostPath `
    -ArgumentList '--lifetime-smoke' `
    -WindowStyle Hidden `
    -Wait `
    -PassThru
if ($process.ExitCode -ne 0) {
    throw "Native lifetime smoke test failed with exit code $($process.ExitCode)."
}

Write-Output "Native XAML lifetime smoke passed: 50 hidden create/teardown cycles ($Architecture, $Configuration)."
