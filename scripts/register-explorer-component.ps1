<#
.SYNOPSIS
  Register or unregister a per-user Explorer Namespace Extension prototype.

.DESCRIPTION
  This script is intentionally limited to the current user's registry hive.
  Explorer loads Shell namespace extensions as in-process COM DLLs. The
  current POC native host is an EXE and must not be passed to this script.

  Register only a native DLL that implements the required Shell/COM contracts.
  Use -WhatIf first. Registration is reversible with -Action Unregister.

.EXAMPLE
  pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 `
    -Action Register `
    -Clsid '{a714cffa-a7b2-49fd-9f15-e42b1aefbca5}' `
    -ServerPath '.\src\ExplorerWelcome.NamespaceExtension\out\x64\Release\ExplorerWelcome.NamespaceExtension.dll' `
    -WhatIf

.EXAMPLE
  pwsh -NoProfile -File .\scripts\register-explorer-component.ps1 `
    -Action Status `
    -Clsid '{a714cffa-a7b2-49fd-9f15-e42b1aefbca5}'
#>
[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = 'High')]
param(
    [ValidateSet('Status', 'Register', 'Unregister')]
    [string]$Action,

    [string]$Clsid = '{a714cffa-a7b2-49fd-9f15-e42b1aefbca5}',

    [string]$ServerPath,

    [string]$DisplayName = 'Views Explorer Welcome',

    [ValidateSet('MyComputer', 'Desktop')]
    [string]$Location = 'MyComputer',

    [switch]$PinToNavigationPane,

    [switch]$Force
)

$ErrorActionPreference = 'Stop'

# The shared menu module is optional. The local fallback keeps this repository
# usable on another workstation without copying shared personal assets here.
$sharedMenuModule = Join-Path ([Environment]::GetFolderPath('UserProfile')) 'OneDrive\Obsidian\70_Assets\PowerShell\Common\modules\menu-utils.ps1'
if (Test-Path -LiteralPath $sharedMenuModule -PathType Leaf) {
    . $sharedMenuModule
}

if (-not (Get-Command -Name Write-MenuItem -ErrorAction SilentlyContinue)) {
    function Write-MenuItem {
        param(
            [Parameter(Mandatory)][string]$Key,
            [Parameter(Mandatory)][string]$Label
        )

        Write-Host ("[{0}] {1}" -f $Key, $Label)
    }
}

if (-not (Get-Command -Name Read-MenuChoice -ErrorAction SilentlyContinue)) {
    function Read-MenuChoice {
        param(
            [string]$Prompt = 'Select',
            [string[]]$AllowedChoices = @()
        )

        while ($true) {
            $choice = Read-Host $Prompt
            if ($AllowedChoices.Count -eq 0 -or $choice -in $AllowedChoices) {
                return $choice
            }

            Write-Host ("Valid choices: {0}" -f ($AllowedChoices -join ', ')) -ForegroundColor Yellow
        }
    }
}

function Invoke-RegistrationMenu {
    Write-Host ''
    Write-Host 'Views Explorer Welcome - per-user registration' -ForegroundColor Cyan
    Write-Host 'The current native host EXE is not a COM Namespace Extension DLL.' -ForegroundColor Yellow
    Write-Host ''
    Write-MenuItem -Key 'R' -Label 'Register a Namespace Extension DLL'
    Write-MenuItem -Key 'U' -Label 'Unregister a Namespace Extension'
    Write-MenuItem -Key 'S' -Label 'Show registration status'
    Write-MenuItem -Key 'Q' -Label 'Exit'

    $choice = Read-MenuChoice -Prompt 'Choice' -AllowedChoices @('R', 'U', 'S', 'Q')
    switch ($choice) {
        'Q' { exit 0 }
        'R' {
            $script:Action = 'Register'
            $script:Clsid = Read-Host 'Namespace Extension CLSID (for example {guid})'
            $script:ServerPath = Read-Host 'Full path to the native COM DLL'
            $script:DisplayName = Read-Host "Display name [$DisplayName]"
            if ([string]::IsNullOrWhiteSpace($script:DisplayName)) {
                $script:DisplayName = 'Views Explorer Welcome'
            }
            do {
                $locationInput = Read-Host "Explorer location (MyComputer or Desktop) [$Location]"
                if ([string]::IsNullOrWhiteSpace($locationInput)) {
                    $script:Location = 'MyComputer'
                    break
                }

                if ($locationInput -in @('MyComputer', 'Desktop')) {
                    $script:Location = $locationInput
                    break
                }

                Write-Host 'Choose MyComputer or Desktop.' -ForegroundColor Yellow
            } while ($true)
            $pinChoice = Read-MenuChoice -Prompt 'Pin in the navigation pane? (Y/N)' -AllowedChoices @('Y', 'N')
            $script:PinToNavigationPane = $pinChoice -eq 'Y'
            $forceChoice = Read-MenuChoice -Prompt 'Overwrite an existing registration if needed? (Y/N)' -AllowedChoices @('Y', 'N')
            $script:Force = $forceChoice -eq 'Y'
        }
        'U' {
            $script:Action = 'Unregister'
            $script:Clsid = Read-Host 'Namespace Extension CLSID (for example {guid})'
            $script:ServerPath = Read-Host 'Optional expected DLL path (press Enter to skip)'
        }
        'S' {
            $script:Action = 'Status'
            $script:Clsid = Read-Host 'Namespace Extension CLSID (for example {guid})'
        }
    }
}

if ([string]::IsNullOrWhiteSpace($Action)) {
    Invoke-RegistrationMenu
}

if ([string]::IsNullOrWhiteSpace($Clsid)) {
    throw '-Clsid is required. Run without parameters to use the interactive menu.'
}

function ConvertTo-NormalizedClsid {
    param([Parameter(Mandatory)][string]$Value)

    try {
        return ([Guid]::Parse($Value)).ToString('B')
    }
    catch {
        throw "CLSID '$Value' is not a valid GUID."
    }
}

function Get-RegistryPaths {
    param(
        [Parameter(Mandatory)][string]$NormalizedClsid,
        [Parameter(Mandatory)][string]$NamespaceLocation
    )

    $clsidPath = "Registry::HKEY_CURRENT_USER\Software\Classes\CLSID\$NormalizedClsid"
    $inprocPath = Join-Path $clsidPath 'InprocServer32'
    $namespacePath = "Registry::HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\$NamespaceLocation\NameSpace\$NormalizedClsid"

    [pscustomobject]@{
        Clsid = $clsidPath
        Inproc = $inprocPath
        Namespace = $namespacePath
    }
}

function Get-DefaultRegistryValue {
    param([Parameter(Mandatory)][string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    try {
        return (Get-Item -LiteralPath $Path).GetValue('')
    }
    catch {
        return $null
    }
}

function Invoke-ShellAssociationRefresh {
    if (-not ('ExplorerWelcomeShellRegistration' -as [type])) {
        Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class ExplorerWelcomeShellRegistration
{
    [DllImport("shell32.dll")]
    public static extern void SHChangeNotify(uint eventId, uint flags, IntPtr item1, IntPtr item2);
}
'@
    }

    [ExplorerWelcomeShellRegistration]::SHChangeNotify(0x08000000, 0, [IntPtr]::Zero, [IntPtr]::Zero)
}

$normalizedClsid = ConvertTo-NormalizedClsid -Value $Clsid
$paths = Get-RegistryPaths -NormalizedClsid $normalizedClsid -NamespaceLocation $Location

switch ($Action) {
    'Status' {
        $registeredServer = Get-DefaultRegistryValue -Path $paths.Inproc
        $namespaceName = Get-DefaultRegistryValue -Path $paths.Namespace

        [pscustomobject]@{
            Action = $Action
            Clsid = $normalizedClsid
            Scope = 'CurrentUser'
            Location = $Location
            ClsidKeyPresent = Test-Path -LiteralPath $paths.Clsid
            NamespaceKeyPresent = Test-Path -LiteralPath $paths.Namespace
            RegisteredServer = $registeredServer
            NamespaceName = $namespaceName
        }
        break
    }

    'Register' {
        if ([string]::IsNullOrWhiteSpace($ServerPath)) {
            throw '-ServerPath is required for Register.'
        }

        $resolvedServerPath = [System.IO.Path]::GetFullPath($ServerPath)
        if (-not (Test-Path -LiteralPath $resolvedServerPath -PathType Leaf)) {
            throw "Server DLL not found: $resolvedServerPath"
        }

        if ([System.IO.Path]::GetExtension($resolvedServerPath) -ine '.dll') {
            throw "Explorer Namespace Extensions must be registered from a COM DLL; refusing '$resolvedServerPath'."
        }

        $existingServer = Get-DefaultRegistryValue -Path $paths.Inproc
        if ($existingServer -and $existingServer -ne $resolvedServerPath -and -not $Force) {
            throw "CLSID $normalizedClsid is already registered to '$existingServer'. Use -Force only after reviewing that registration."
        }

        if ($PSCmdlet.ShouldProcess("HKCU Explorer $Location", "Register $normalizedClsid to $resolvedServerPath")) {
            New-Item -Path $paths.Clsid -Force | Out-Null
            New-ItemProperty -LiteralPath $paths.Clsid -Name '(default)' -Value $DisplayName -PropertyType String -Force | Out-Null

            New-Item -Path $paths.Inproc -Force | Out-Null
            New-ItemProperty -LiteralPath $paths.Inproc -Name '(default)' -Value $resolvedServerPath -PropertyType String -Force | Out-Null
            New-ItemProperty -LiteralPath $paths.Inproc -Name 'ThreadingModel' -Value 'Apartment' -PropertyType String -Force | Out-Null

            if ($PinToNavigationPane) {
                New-ItemProperty -LiteralPath $paths.Clsid -Name 'System.IsPinnedToNameSpaceTree' -PropertyType DWord -Value 1 -Force | Out-Null
            }

            New-Item -Path $paths.Namespace -Force | Out-Null
            New-ItemProperty -LiteralPath $paths.Namespace -Name '(default)' -Value $DisplayName -PropertyType String -Force | Out-Null

            Invoke-ShellAssociationRefresh
            Write-Output "Registered per-user Explorer Namespace Extension $normalizedClsid at $resolvedServerPath."
            Write-Output 'Restart File Explorer manually if the new entry is not visible immediately.'
        }
        break
    }

    'Unregister' {
        $registeredServer = Get-DefaultRegistryValue -Path $paths.Inproc
        if (-not (Test-Path -LiteralPath $paths.Clsid) -and -not (Test-Path -LiteralPath $paths.Namespace)) {
            Write-Output "No per-user registration found for $normalizedClsid."
            break
        }

        if ($registeredServer -and $ServerPath) {
            $resolvedServerPath = [System.IO.Path]::GetFullPath($ServerPath)
            if ($registeredServer -ne $resolvedServerPath -and -not $Force) {
                throw "The registered server is '$registeredServer', not '$resolvedServerPath'. Use -Force only after reviewing that registration."
            }
        }

        if ($PSCmdlet.ShouldProcess("HKCU Explorer $Location", "Unregister $normalizedClsid")) {
            if (Test-Path -LiteralPath $paths.Namespace) {
                Remove-Item -LiteralPath $paths.Namespace -Recurse -Force
            }

            if (Test-Path -LiteralPath $paths.Clsid) {
                Remove-Item -LiteralPath $paths.Clsid -Recurse -Force
            }

            Invoke-ShellAssociationRefresh
            Write-Output "Unregistered per-user Explorer Namespace Extension $normalizedClsid."
            Write-Output 'Restart File Explorer manually if the old entry is still visible.'
        }
        break
    }
}
