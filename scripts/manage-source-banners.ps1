<#
===============================================================================
Views Explorer Welcome POC
File: scripts/manage-source-banners.ps1
Purpose: Manages canonical source banners across repository-owned source files.

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
Manage canonical source banners for repository-owned source files.

.DESCRIPTION
With no parameters, opens an interactive menu. For automation, pass -Action
Preview, Apply, Verify, or Remove. The script supports C#, PowerShell, C, and
C++ sources, migrates the POC's earlier short banner, preserves common source
encodings and newline style, and ignores generated, build, vendor, and
third-party paths.

.PARAMETER Action
Menu, Preview, Apply, Verify, or Remove. Menu is available only when no
parameters are supplied.

.PARAMETER RepositoryRoot
Repository root. Defaults to the parent directory of this script's folder.

.PARAMETER SourceRoot
One or more repository-relative directories to scan. Defaults to the complete
repository.

.PARAMETER NoProgress
Suppress progress output for automation and redirected output.

.PARAMETER Help
Display command help.

.EXAMPLE
pwsh -NoProfile -File .\scripts\manage-source-banners.ps1

Open the interactive menu.

.EXAMPLE
pwsh -NoProfile -File .\scripts\manage-source-banners.ps1 -Action Preview

Preview additions and legacy-banner replacements without writing files.

.EXAMPLE
pwsh -NoProfile -File .\scripts\manage-source-banners.ps1 -Action Apply -Confirm:$false

Apply canonical banners non-interactively.

.EXAMPLE
pwsh -NoProfile -File .\scripts\manage-source-banners.ps1 -Action Verify -NoProgress

Fail when an owned source file lacks a valid canonical banner.
#>

[CmdletBinding(SupportsShouldProcess, ConfirmImpact = "Medium")]
param(
    [ValidateSet("Menu", "Preview", "Apply", "Verify", "Remove")]
    [string]$Action = "Menu",

    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),

    [string[]]$SourceRoot = @("."),

    [switch]$NoProgress,

    [Alias("h")]
    [switch]$Help
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectName = "Views Explorer Welcome POC"
$copyright = "Copyright (c) 2026 Umberto Giacobbi"
$author = "Umberto Giacobbi"
$repositoryUrl = "https://github.com/umbertotechnopreneur/views-explorer-welcome-poc"
$licenseName = "PolyForm Noncommercial License 1.0.0"
$spdxIdentifier = "PolyForm-Noncommercial-1.0.0"
$openSourceUrl = "https://umbertogiacobbi.biz/opensource"
$canonicalMarker = "SPDX-License-Identifier: $spdxIdentifier"

$supportedExtensions = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
@(".cs", ".ps1", ".psm1", ".psd1", ".c", ".cpp", ".h", ".hpp") |
    ForEach-Object { [void]$supportedExtensions.Add($_) }

$excludedSegments = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
@(
    ".git",
    ".vs",
    ".vscode",
    ".worktrees",
    "artifacts",
    "bin",
    "build",
    "external",
    "externals",
    "generated",
    "node_modules",
    "obj",
    "out",
    "packages",
    "third-party",
    "third_party",
    "vendor",
    "vendors"
) | ForEach-Object { [void]$excludedSegments.Add($_) }

$generatedMarkerPattern = '(?im)(auto(?:matically)?[- ]generated|generated file|this file is generated|do not edit)'
$legacyBannerPattern = '(?s)\A// -{20,}\r?\n// Views Explorer Welcome POC\r?\n(?<purpose>(?:(?!// -{20,}(?:\r?\n|$))//[^\r\n]*(?:\r?\n|$)){1,8})// -{20,}(?:\r?\n){0,2}'

function Get-RepositoryPath {
    param([Parameter(Mandatory)][string]$Path)

    $resolved = [System.IO.Path]::GetFullPath($Path).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    )

    if (-not (Test-Path -LiteralPath $resolved -PathType Container)) {
        throw "Repository root does not exist: $resolved"
    }

    return $resolved
}

function Get-RelativeSourcePath {
    param(
        [Parameter(Mandatory)][string]$RepositoryPath,
        [Parameter(Mandatory)][string]$Path
    )

    return [System.IO.Path]::GetRelativePath($RepositoryPath, $Path).Replace('\', '/')
}

function Test-ExcludedRelativePath {
    param([Parameter(Mandatory)][string]$RelativePath)

    foreach ($segment in ($RelativePath -split '/')) {
        if ($excludedSegments.Contains($segment)) {
            return $true
        }
    }

    return $false
}

function Get-OwnedSourceFiles {
    param(
        [Parameter(Mandatory)][string]$RepositoryPath,
        [Parameter(Mandatory)][string[]]$RelativeRoots
    )

    $repositoryPrefix = $RepositoryPath + [System.IO.Path]::DirectorySeparatorChar
    $filesByPath = [System.Collections.Generic.Dictionary[string, System.IO.FileInfo]]::new(
        [System.StringComparer]::OrdinalIgnoreCase
    )

    foreach ($relativeRoot in $RelativeRoots) {
        $rootPath = [System.IO.Path]::GetFullPath((Join-Path $RepositoryPath $relativeRoot))
        $isRepositoryRoot = $rootPath.Equals(
            $RepositoryPath,
            [System.StringComparison]::OrdinalIgnoreCase
        )
        $isInsideRepository = $rootPath.StartsWith(
            $repositoryPrefix,
            [System.StringComparison]::OrdinalIgnoreCase
        )

        if (-not $isRepositoryRoot -and -not $isInsideRepository) {
            throw "Source root escapes the repository: $relativeRoot"
        }
        if (-not (Test-Path -LiteralPath $rootPath -PathType Container)) {
            throw "Source root does not exist: $relativeRoot"
        }

        $rootItem = Get-Item -LiteralPath $rootPath -Force
        if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw "Source root cannot be a reparse point: $relativeRoot"
        }

        $rootRelativePath = Get-RelativeSourcePath -RepositoryPath $RepositoryPath -Path $rootPath
        if (-not $isRepositoryRoot -and (Test-ExcludedRelativePath -RelativePath $rootRelativePath)) {
            throw "Source root is excluded by policy: $relativeRoot"
        }

        $pending = [System.Collections.Generic.Stack[System.IO.DirectoryInfo]]::new()
        $pending.Push($rootItem)

        while ($pending.Count -gt 0) {
            $directory = $pending.Pop()

            foreach ($file in $directory.EnumerateFiles()) {
                $relativePath = Get-RelativeSourcePath -RepositoryPath $RepositoryPath -Path $file.FullName
                $isReparsePoint = (
                    $file.Attributes -band [System.IO.FileAttributes]::ReparsePoint
                ) -ne 0
                if (
                    -not $isReparsePoint -and
                    $supportedExtensions.Contains($file.Extension) -and
                    -not (Test-ExcludedRelativePath -RelativePath $relativePath)
                ) {
                    $filesByPath[$file.FullName] = $file
                }
            }

            foreach ($child in $directory.EnumerateDirectories()) {
                $relativePath = Get-RelativeSourcePath -RepositoryPath $RepositoryPath -Path $child.FullName
                $isReparsePoint = (
                    $child.Attributes -band [System.IO.FileAttributes]::ReparsePoint
                ) -ne 0

                if (
                    -not $isReparsePoint -and
                    -not (Test-ExcludedRelativePath -RelativePath $relativePath)
                ) {
                    $pending.Push($child)
                }
            }
        }
    }

    return @(
        $filesByPath.Values |
            Sort-Object -Property @{
                Expression = {
                    if ($_.FullName.EndsWith(
                        "scripts\manage-source-banners.ps1",
                        [System.StringComparison]::OrdinalIgnoreCase
                    )) {
                        1
                    }
                    else {
                        0
                    }
                }
            }, FullName
    )
}

function Read-SourceText {
    param([Parameter(Mandatory)][string]$Path)

    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $offset = 0

    if (
        $bytes.Length -ge 3 -and
        $bytes[0] -eq 0xEF -and
        $bytes[1] -eq 0xBB -and
        $bytes[2] -eq 0xBF
    ) {
        $encoding = [System.Text.UTF8Encoding]::new($true, $true)
        $offset = 3
    }
    elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
        $encoding = [System.Text.UnicodeEncoding]::new($false, $true, $true)
        $offset = 2
    }
    elseif ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFE -and $bytes[1] -eq 0xFF) {
        $encoding = [System.Text.UnicodeEncoding]::new($true, $true, $true)
        $offset = 2
    }
    else {
        $encoding = [System.Text.UTF8Encoding]::new($false, $true)
    }

    try {
        $text = $encoding.GetString($bytes, $offset, $bytes.Length - $offset)
    }
    catch {
        throw "Unsupported or invalid text encoding in '$Path': $($_.Exception.Message)"
    }

    return [pscustomobject]@{
        Text = $text
        Encoding = $encoding
    }
}

function Get-SourceLayout {
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][string]$Extension
    )

    $newline = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $prefix = ""
    $body = $Text

    if (
        $Extension -in ".ps1", ".psm1", ".psd1" -and
        $Text.StartsWith("#!", [System.StringComparison]::Ordinal)
    ) {
        $lineEnd = $Text.IndexOf("`n", [System.StringComparison]::Ordinal)
        if ($lineEnd -lt 0) {
            $prefix = $Text + $newline
            $body = ""
        }
        else {
            $prefix = $Text.Substring(0, $lineEnd + 1)
            $body = $Text.Substring($lineEnd + 1)
        }
    }

    return [pscustomobject]@{
        Newline = $newline
        Prefix = $prefix
        Body = $body
    }
}

function ConvertTo-DisplayWords {
    param([Parameter(Mandatory)][string]$Value)

    $words = $Value -replace '[_-]+', ' '
    $words = $words -replace '(?<=[a-z0-9])(?=[A-Z])', ' '
    $words = $words -replace '(?<=[A-Z])(?=[A-Z][a-z])', ' '
    return ($words -replace '\s+', ' ').Trim()
}

function Get-InferredPurpose {
    param(
        [Parameter(Mandatory)][System.IO.FileInfo]$File,
        [Parameter(Mandatory)][string]$Text
    )

    if ($File.Extension -in ".ps1", ".psm1", ".psd1") {
        $synopsisMatch = [regex]::Match(
            $Text,
            '(?im)^\s*\.SYNOPSIS\s*\r?\n\s*(?<synopsis>[^\r\n]+)'
        )
        if ($synopsisMatch.Success) {
            return $synopsisMatch.Groups["synopsis"].Value.Trim().TrimEnd(".") + "."
        }
    }

    $name = ConvertTo-DisplayWords -Value $File.BaseName
    switch ($File.Extension.ToLowerInvariant()) {
        ".cs" {
            return "Defines $name behavior for the Views Explorer Welcome POC."
        }
        { $_ -in ".ps1", ".psm1", ".psd1" } {
            return "Provides the $name repository workflow."
        }
        { $_ -in ".c", ".cpp" } {
            return "Implements $name behavior for the native Explorer experiment."
        }
        default {
            return "Declares $name behavior for the native Explorer experiment."
        }
    }
}

function Get-BannerText {
    param(
        [Parameter(Mandatory)][string]$RelativePath,
        [Parameter(Mandatory)][string]$Purpose,
        [Parameter(Mandatory)][string]$Extension,
        [Parameter(Mandatory)][string]$Newline
    )

    $fields = @(
        $projectName,
        "File: $RelativePath",
        "Purpose: $Purpose",
        "",
        $copyright,
        "Author: $author",
        "Repository: $repositoryUrl",
        "License: $licenseName",
        $canonicalMarker,
        "Open source: $openSourceUrl"
    )

    switch ($Extension.ToLowerInvariant()) {
        { $_ -in ".ps1", ".psm1", ".psd1" } {
            $lines = @("<#", "===============================================================================")
            $lines += $fields
            $lines += @("===============================================================================", "#>")
        }
        { $_ -in ".c", ".h" } {
            $lines = @("/* =============================================================================")
            $lines += $fields | ForEach-Object {
                if ([string]::IsNullOrEmpty($_)) { " *" } else { " * $_" }
            }
            $lines += @(" * =============================================================================", " */")
        }
        default {
            $lines = @("// =============================================================================")
            $lines += $fields | ForEach-Object {
                if ([string]::IsNullOrEmpty($_)) { "//" } else { "// $_" }
            }
            $lines += "// ============================================================================="
        }
    }

    return $lines -join $Newline
}

function Get-LegacyBanner {
    param([Parameter(Mandatory)][string]$Body)

    $match = [regex]::Match($Body, $legacyBannerPattern)
    if (-not $match.Success) {
        return $null
    }

    $purposeLines = @(
        $match.Groups["purpose"].Value -split '\r?\n' |
            ForEach-Object { ($_ -replace '^//\s?', '').Trim() } |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    )

    return [pscustomobject]@{
        Length = $match.Length
        Purpose = ($purposeLines -join " ")
    }
}

function Get-CanonicalBannerCandidate {
    param(
        [Parameter(Mandatory)][string]$Body,
        [Parameter(Mandatory)][string]$Extension
    )

    if ($Extension -in ".ps1", ".psm1", ".psd1") {
        if (-not $Body.StartsWith("<#", [System.StringComparison]::Ordinal)) {
            return $null
        }
        $end = $Body.IndexOf("#>", [System.StringComparison]::Ordinal)
        if ($end -lt 0) {
            return $null
        }
        $length = $end + 2
    }
    elseif ($Extension -in ".c", ".h") {
        if (-not $Body.StartsWith("/*", [System.StringComparison]::Ordinal)) {
            return $null
        }
        $end = $Body.IndexOf("*/", [System.StringComparison]::Ordinal)
        if ($end -lt 0) {
            return $null
        }
        $length = $end + 2
    }
    else {
        if (-not $Body.StartsWith("// ===============", [System.StringComparison]::Ordinal)) {
            return $null
        }
        $match = [regex]::Match(
            $Body,
            '(?s)\A// ={20,}\r?\n.*?\r?\n// ={20,}'
        )
        if (-not $match.Success) {
            return $null
        }
        $length = $match.Length
    }

    $text = $Body.Substring(0, $length)
    if (-not $text.Contains($canonicalMarker, [System.StringComparison]::Ordinal)) {
        return $null
    }

    $purposeMatch = [regex]::Match(
        $text,
        '(?im)^(?:(?://| \*)\s*)?Purpose:\s*(?<purpose>[^\r\n]+)'
    )
    $purpose = if ($purposeMatch.Success) {
        $purposeMatch.Groups["purpose"].Value.Trim()
    }
    else {
        ""
    }

    return [pscustomobject]@{
        Length = $length
        Purpose = $purpose
        Text = $text
    }
}

function Remove-LeadingGap {
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][string]$Newline
    )

    if ($Text.StartsWith($Newline + $Newline, [System.StringComparison]::Ordinal)) {
        return $Text.Substring(($Newline + $Newline).Length)
    }
    if ($Text.StartsWith($Newline, [System.StringComparison]::Ordinal)) {
        return $Text.Substring($Newline.Length)
    }

    return $Text
}

function Get-FileBannerState {
    param(
        [Parameter(Mandatory)][System.IO.FileInfo]$File,
        [Parameter(Mandatory)][string]$RelativePath,
        [Parameter(Mandatory)]$Layout
    )

    $canonical = Get-CanonicalBannerCandidate -Body $Layout.Body -Extension $File.Extension
    $legacy = Get-LegacyBanner -Body $Layout.Body
    $purpose = if ($null -ne $canonical -and -not [string]::IsNullOrWhiteSpace($canonical.Purpose)) {
        $canonical.Purpose
    }
    elseif ($null -ne $legacy -and -not [string]::IsNullOrWhiteSpace($legacy.Purpose)) {
        $legacy.Purpose
    }
    else {
        Get-InferredPurpose -File $File -Text $Layout.Body
    }

    $expected = Get-BannerText `
        -RelativePath $RelativePath `
        -Purpose $purpose `
        -Extension $File.Extension `
        -Newline $Layout.Newline

    $markerCount = [regex]::Matches(
        $Layout.Prefix + $Layout.Body,
        [regex]::Escape($canonicalMarker)
    ).Count
    $kind = if ($null -ne $canonical -and $canonical.Text.Equals(
        $expected,
        [System.StringComparison]::Ordinal
    ) -and $markerCount -eq 1) {
        "Canonical"
    }
    elseif ($null -ne $canonical) {
        "NonCanonical"
    }
    elseif ($null -ne $legacy) {
        "Legacy"
    }
    else {
        "Missing"
    }

    return [pscustomobject]@{
        Kind = $kind
        Purpose = $purpose
        Expected = $expected
        Canonical = $canonical
        Legacy = $legacy
        MarkerCount = $markerCount
    }
}

function Write-Outcome {
    param(
        [Parameter(Mandatory)][string]$Status,
        [Parameter(Mandatory)][string]$RelativePath
    )

    Write-Host ("{0,-18} {1}" -f $Status, $RelativePath)
}

function Invoke-SourceBannerWorkflow {
    param(
        [Parameter(Mandatory)][string]$SelectedAction,
        [Parameter(Mandatory)][string]$RepositoryPath,
        [Parameter(Mandatory)][string[]]$RelativeRoots,
        [switch]$SuppressProgress
    )

    $files = @(Get-OwnedSourceFiles -RepositoryPath $RepositoryPath -RelativeRoots $RelativeRoots)
    $summary = [ordered]@{
        Action = $SelectedAction
        Candidates = $files.Count
        Planned = 0
        Updated = 0
        Valid = 0
        Issues = 0
        SkippedGenerated = 0
    }

    for ($index = 0; $index -lt $files.Count; $index++) {
        $file = $files[$index]
        $relativePath = Get-RelativeSourcePath -RepositoryPath $RepositoryPath -Path $file.FullName

        if (-not $SuppressProgress) {
            $percent = if ($files.Count -eq 0) {
                100
            }
            else {
                [Math]::Floor((($index + 1) / $files.Count) * 100)
            }
            Write-Progress `
                -Id 1 `
                -Activity "Source banners: $SelectedAction" `
                -Status "$($index + 1) of $($files.Count): $relativePath" `
                -PercentComplete $percent
        }

        $source = Read-SourceText -Path $file.FullName
        $layout = Get-SourceLayout -Text $source.Text -Extension $file.Extension
        $prefixLength = [Math]::Min(4096, $layout.Body.Length)
        $prefixText = $layout.Body.Substring(0, $prefixLength)
        $state = Get-FileBannerState -File $file -RelativePath $relativePath -Layout $layout

        if (
            $state.Kind -eq "Missing" -and
            $prefixText -match $generatedMarkerPattern
        ) {
            Write-Outcome -Status "SKIP generated" -RelativePath $relativePath
            $summary.SkippedGenerated++
            continue
        }

        if ($SelectedAction -eq "Verify") {
            if ($state.Kind -eq "Canonical") {
                Write-Outcome -Status "VALID" -RelativePath $relativePath
                $summary.Valid++
            }
            else {
                Write-Outcome -Status "INVALID $($state.Kind)" -RelativePath $relativePath
                $summary.Issues++
            }
            continue
        }

        if ($SelectedAction -eq "Remove") {
            if ($state.Kind -ne "Canonical") {
                Write-Outcome -Status "SKIP no banner" -RelativePath $relativePath
                continue
            }

            $summary.Planned++
            if (-not $PSCmdlet.ShouldProcess($relativePath, "Remove canonical source banner")) {
                Write-Outcome -Status "PLAN remove" -RelativePath $relativePath
                continue
            }

            $remaining = $layout.Body.Substring($state.Canonical.Length)
            $remaining = Remove-LeadingGap -Text $remaining -Newline $layout.Newline
            [System.IO.File]::WriteAllText(
                $file.FullName,
                $layout.Prefix + $remaining,
                $source.Encoding
            )
            Write-Outcome -Status "REMOVED" -RelativePath $relativePath
            $summary.Updated++
            continue
        }

        if ($state.Kind -eq "Canonical") {
            Write-Outcome -Status "SKIP canonical" -RelativePath $relativePath
            $summary.Valid++
            continue
        }

        $summary.Planned++
        $operation = if ($state.Kind -in "Legacy", "NonCanonical") {
            "replace"
        }
        else {
            "add"
        }

        if ($SelectedAction -eq "Preview") {
            Write-Outcome -Status "PLAN $operation" -RelativePath $relativePath
            continue
        }

        if (-not $PSCmdlet.ShouldProcess($relativePath, "$operation canonical source banner")) {
            Write-Outcome -Status "PLAN $operation" -RelativePath $relativePath
            continue
        }

        $remaining = switch ($state.Kind) {
            "Legacy" {
                $layout.Body.Substring($state.Legacy.Length)
            }
            "NonCanonical" {
                $afterBanner = $layout.Body.Substring($state.Canonical.Length)
                Remove-LeadingGap -Text $afterBanner -Newline $layout.Newline
            }
            default {
                $layout.Body
            }
        }

        $updatedText = (
            $layout.Prefix +
            $state.Expected +
            $layout.Newline +
            $layout.Newline +
            $remaining
        )
        [System.IO.File]::WriteAllText($file.FullName, $updatedText, $source.Encoding)
        Write-Outcome -Status "UPDATED $operation" -RelativePath $relativePath
        $summary.Updated++
    }

    if (-not $SuppressProgress) {
        Write-Progress -Id 1 -Activity "Source banners: $SelectedAction" -Completed
    }

    Write-Host ""
    foreach ($entry in $summary.GetEnumerator()) {
        Write-Host ("{0}: {1}" -f $entry.Key, $entry.Value)
    }

    if ($SelectedAction -eq "Preview" -and $summary.Planned -gt 0) {
        Write-Host ""
        Write-Host "Preview only. Re-run with -Action Apply -Confirm:`$false to write changes."
    }

    if ($SelectedAction -eq "Verify" -and $summary.Issues -gt 0) {
        throw "Source banner verification failed for $($summary.Issues) file(s)."
    }

    return [pscustomobject]$summary
}

function Show-SourceBannerMenu {
    Write-Host ""
    Write-Host $projectName
    Write-Host "Source banner manager"
    Write-Host ""
    Write-Host "  1. Preview changes"
    Write-Host "  2. Apply changes"
    Write-Host "  3. Verify banners"
    Write-Host "  4. Remove canonical banners"
    Write-Host "  0. Exit"
    Write-Host ""

    switch (Read-Host "Select an action") {
        "1" { return "Preview" }
        "2" { return "Apply" }
        "3" { return "Verify" }
        "4" { return "Remove" }
        "0" { return $null }
        default { throw "Unknown menu selection." }
    }
}

if ($Help) {
    Get-Help -Full $PSCommandPath
    return
}

$noArguments = $PSBoundParameters.Count -eq 0
if ($Action -eq "Menu") {
    if (-not $noArguments) {
        throw "Menu mode is available only without parameters. Pass -Action Preview, Apply, Verify, or Remove."
    }

    $selectedAction = Show-SourceBannerMenu
    if ($null -eq $selectedAction) {
        Write-Host "No changes made."
        return
    }

    if ($selectedAction -in "Apply", "Remove") {
        $confirmation = Read-Host "Type $($selectedAction.ToUpperInvariant()) to continue"
        if (-not $confirmation.Equals(
            $selectedAction.ToUpperInvariant(),
            [System.StringComparison]::Ordinal
        )) {
            Write-Host "No changes made."
            return
        }
    }
    $Action = $selectedAction
}

$resolvedRepositoryPath = Get-RepositoryPath -Path $RepositoryRoot
[void](Invoke-SourceBannerWorkflow `
    -SelectedAction $Action `
    -RepositoryPath $resolvedRepositoryPath `
    -RelativeRoots $SourceRoot `
    -SuppressProgress:$NoProgress)
