<#
.SYNOPSIS
  Read-only toolchain check for the Explorer Welcome POC.
#>
[CmdletBinding()]
param([switch]$Help)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'

function Test-Tool([string]$Name) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    [pscustomobject]@{ Tool = $Name; Available = [bool]$command; Path = if ($command) { $command.Source } else { '' } }
}

$results = @(
    (Test-Tool 'dotnet'),
    (Test-Tool 'git'),
    (Test-Tool 'pwsh'),
    [pscustomobject]@{ Tool = 'MSBuild'; Available = Test-Path 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe'; Path = 'C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe' },
    [pscustomobject]@{ Tool = 'MSVC v145'; Available = Test-Path 'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe'; Path = 'C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Tools\MSVC\14.51.36231' },
    [pscustomobject]@{ Tool = 'Windows SDK'; Available = Test-Path 'C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\windows.h'; Path = '10.0.26100.0' },
    [pscustomobject]@{ Tool = 'C++/WinRT'; Available = Test-Path 'C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\cppwinrt\winrt\base.h'; Path = '10.0.26100.0' },
    [pscustomobject]@{ Tool = 'XAML Island header'; Available = Test-Path 'C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um\windows.ui.xaml.hosting.desktopwindowxamlsource.h'; Path = 'DesktopWindowXamlSource' }
)

$results | Format-Table -AutoSize
if ($results | Where-Object { -not $_.Available }) { exit 1 }
