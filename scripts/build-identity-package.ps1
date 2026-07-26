<#
.SYNOPSIS
  Build the unsigned identity-only package-with-external-location template.

.DESCRIPTION
  This package can grant identity to the standalone out-of-process host. It
  does not register or package the in-process Explorer Namespace Extension.
#>
[CmdletBinding()]
param(
    [switch]$Help,
    [string]$OutputPath
)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$layout = Join-Path $root 'packaging\identity'
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root 'artifacts\packaging\ViewsExplorerWelcome.Identity.msix'
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
if ([System.IO.Path]::GetExtension($resolvedOutput) -ine '.msix') {
    throw 'The output path must use the .msix extension.'
}

$makeAppx = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' `
    -Recurse `
    -Filter 'makeappx.exe' `
    -ErrorAction SilentlyContinue |
    Where-Object { $_.Directory.Name -eq 'x64' } |
    Sort-Object FullName -Descending |
    Select-Object -First 1

if (-not $makeAppx) {
    throw 'MakeAppx.exe was not found. Install the Windows SDK packaging tools.'
}

$outputDirectory = Split-Path -Parent $resolvedOutput
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

& $makeAppx.FullName pack /o /d $layout /nv /p $resolvedOutput
if ($LASTEXITCODE -ne 0) {
    throw "MakeAppx failed with exit code $LASTEXITCODE."
}

Write-Output "Built unsigned identity-only package: $resolvedOutput"
Write-Output 'No package was signed, trusted, installed, or registered.'
