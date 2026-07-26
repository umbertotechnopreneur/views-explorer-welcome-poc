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

$native = Join-Path $root 'src\ExplorerWelcome.NativeHost\ExplorerWelcome.NativeHost.vcxproj'
& $msbuild $native "/p:Configuration=$Configuration" "/p:Platform=$Architecture" "/p:PlatformToolset=$PlatformToolset" /m /nologo
if ($LASTEXITCODE -ne 0) { throw "Native build failed for $Architecture" }
