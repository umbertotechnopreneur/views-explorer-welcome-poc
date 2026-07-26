<#
===============================================================================
Views Explorer Welcome POC
File: scripts/validate-boundaries.ps1
Purpose: Enforce static safety boundaries for the Explorer-facing native code.

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
  Enforce static safety boundaries for the Explorer-facing native code.
#>
[CmdletBinding()]
param([switch]$Help)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$namespaceSource = Join-Path $root 'src\ExplorerWelcome.NamespaceExtension\namespace_extension.cpp'
$viewSource = Join-Path $root 'src\ExplorerWelcome.NativeUi\welcome_page.h'
$packageManifest = Join-Path $root 'packaging\identity\AppxManifest.xml'

$forbidden = @(
    [pscustomobject]@{
        Scope = 'Namespace Extension'
        Path = $namespaceSource
        Patterns = @(
            'IContextMenu',
            'IExplorerCommand',
            'ShellExecute',
            'CreateProcess',
            'WinHttp',
            'URLDownloadToFile',
            'GetDiskFreeSpaceEx',
            'std::filesystem'
        )
    },
    [pscustomobject]@{
        Scope = 'Stationary XAML interactions'
        Path = $viewSource
        Patterns = @(
            'TranslateTransform',
            'CompositeTransform',
            'RenderTransform',
            'PointerEntered',
            'PointerExited'
        )
    },
    [pscustomobject]@{
        Scope = 'Identity-only package'
        Path = $packageManifest
        Patterns = @(
            'windows.comServer',
            'fileExplorerContextMenus',
            'ExplorerWelcome.NamespaceExtension.dll'
        )
    }
)

$violations = foreach ($rule in $forbidden) {
    foreach ($pattern in $rule.Patterns) {
        if (Select-String -LiteralPath $rule.Path -SimpleMatch -Quiet -Pattern $pattern) {
            [pscustomobject]@{
                Scope = $rule.Scope
                File = [System.IO.Path]::GetRelativePath($root, $rule.Path)
                Pattern = $pattern
            }
        }
    }
}

$required = @(
    [pscustomobject]@{ Path = $namespaceSource; Pattern = 'BrowseObject' },
    [pscustomobject]@{ Path = $namespaceSource; Pattern = 'SBSP_SAMEBROWSER' },
    [pscustomobject]@{ Path = $namespaceSource; Pattern = 'DesktopWindowXamlSource' },
    [pscustomobject]@{ Path = $viewSource; Pattern = 'AutomationProperties::SetName' },
    [pscustomobject]@{ Path = $viewSource; Pattern = 'pageAlive' }
)

$missing = foreach ($rule in $required) {
    if (-not (Select-String -LiteralPath $rule.Path -SimpleMatch -Quiet -Pattern $rule.Pattern)) {
        [pscustomobject]@{
            Scope = 'Required safety marker'
            File = [System.IO.Path]::GetRelativePath($root, $rule.Path)
            Pattern = $rule.Pattern
        }
    }
}

if ($violations) {
    $violations | Format-Table -AutoSize
    throw 'Forbidden Explorer-boundary pattern found.'
}
if ($missing) {
    $missing | Format-Table -AutoSize
    throw 'Required Explorer-boundary marker is missing.'
}

Write-Output 'Explorer boundary validation passed.'
