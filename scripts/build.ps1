<#
===============================================================================
Views Explorer Welcome POC
File: scripts/build.ps1
Purpose: Build managed projects and the native XAML host for one explicit architecture.

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
  Build managed projects and the native XAML host for one explicit architecture.
#>
[CmdletBinding()]
param(
    [switch]$Help,
    [ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$PlatformToolset = 'v145'
)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$msbuildCommand = Get-Command msbuild -ErrorAction SilentlyContinue
$msbuild = if ($msbuildCommand) { $msbuildCommand.Source } else {
    @(
        'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $msbuild) { throw 'MSBuild not found. Install Visual Studio C++ tools or run setup-msbuild first.' }

$projects = @(
    'src\ExplorerWelcome.Contracts\ExplorerWelcome.Contracts.csproj',
    'src\ExplorerWelcome.Broker\ExplorerWelcome.Broker.csproj',
    'src\ExplorerWelcome.HeavyApp\ExplorerWelcome.HeavyApp.csproj'
)
foreach ($project in $projects) {
    & dotnet build (Join-Path $root $project) --nologo --configuration $Configuration
    if ($LASTEXITCODE -ne 0) { throw "dotnet build failed: $project" }
}

$nativeProjects = @(
    'src\ExplorerWelcome.NativeHost\ExplorerWelcome.NativeHost.vcxproj',
    'src\ExplorerWelcome.NamespaceExtension\ExplorerWelcome.NamespaceExtension.vcxproj'
)
foreach ($nativeProject in $nativeProjects) {
    $projectPath = Join-Path $root $nativeProject
    & $msbuild $projectPath "/p:Configuration=$Configuration" "/p:Platform=$Architecture" "/p:PlatformToolset=$PlatformToolset" /m /nologo
    if ($LASTEXITCODE -ne 0) { throw "Native build failed for $Architecture`: $nativeProject" }
}
