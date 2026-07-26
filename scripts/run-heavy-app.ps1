<#
===============================================================================
Views Explorer Welcome POC
File: scripts/run-heavy-app.ps1
Purpose: Run the separate heavy-app contract client.

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
  Run the separate heavy-app contract client.
#>
[CmdletBinding()]
param([switch]$Help)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& dotnet run --project (Join-Path $root 'src\ExplorerWelcome.HeavyApp\ExplorerWelcome.HeavyApp.csproj')
exit $LASTEXITCODE
