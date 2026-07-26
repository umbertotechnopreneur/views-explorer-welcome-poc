<#
.SYNOPSIS
  Build managed projects and the native XAML host for one explicit architecture.
#>
[CmdletBinding()]
param(
    [switch]$Help,
    [ValidateSet('x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$msbuild = 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) { throw "MSBuild not found: $msbuild" }

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
& $msbuild $native "/p:Configuration=$Configuration" "/p:Platform=$Architecture" /m /nologo
if ($LASTEXITCODE -ne 0) { throw "Native build failed for $Architecture" }
