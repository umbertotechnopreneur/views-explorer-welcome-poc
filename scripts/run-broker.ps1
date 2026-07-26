<#
===============================================================================
Views Explorer Welcome POC
File: scripts/run-broker.ps1
Purpose: Run the out-of-process broker in the foreground.

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
  Run the out-of-process broker in the foreground.
#>
[CmdletBinding()]
param([switch]$Help)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& dotnet run --project (Join-Path $root 'src\ExplorerWelcome.Broker\ExplorerWelcome.Broker.csproj')
exit $LASTEXITCODE
