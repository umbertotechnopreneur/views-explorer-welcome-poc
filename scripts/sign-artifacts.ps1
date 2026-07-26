<#
===============================================================================
Views Explorer Welcome POC
File: scripts/sign-artifacts.ps1
Purpose: Sign or inspect repository build artifacts using a certificate-store thumbprint.

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
  Sign or inspect repository build artifacts using a certificate-store thumbprint.

.DESCRIPTION
  The script never accepts a PFX password. It selects a code-signing certificate
  from CurrentUser\My by public thumbprint, keeping Vault secrets and passwords
  out of process command lines and logs.

.EXAMPLE
  pwsh -NoProfile -File .\scripts\sign-artifacts.ps1 -Action Sign -Architecture x64

.EXAMPLE
  pwsh -NoProfile -File .\scripts\sign-artifacts.ps1 -Action Verify -Architecture All
#>
[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'Medium')]
param(
    [ValidateSet('Menu', 'Sign', 'Verify', 'Status')]
    [string]$Action = 'Menu',
    [ValidateSet('x64', 'ARM64', 'All')]
    [string]$Architecture = 'All',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string[]]$Path,
    [string]$CertificateThumbprint,
    [string]$VaultRoot = (Join-Path $env:USERPROFILE 'OneDrive\Obsidian\Vault'),
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9 ._-]{2,100}$')]
    [string]$ProfileName = 'Views Explorer Welcome POC Development',
    [string]$TimestampUrl,
    [switch]$RequireTimestamp,
    [switch]$Help
)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$profileDirectory = Join-Path $VaultRoot 'Views Explorer Welcome POC\Signing'
$metadataPath = Join-Path $profileDirectory "$ProfileName.metadata.json"
$allowedExtensions = @('.dll', '.exe', '.msi', '.msix', '.msixbundle')

if ($null -eq ('ViewsExplorer.Signing.AuthenticodeTrust' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;

namespace ViewsExplorer.Signing
{
    public static class AuthenticodeTrust
    {
        public const int S_OK = 0;
        public const int TRUST_E_NOSIGNATURE = unchecked((int)0x800B0100);
        public const int CERT_E_UNTRUSTEDROOT = unchecked((int)0x800B0109);
        public const int TRUST_E_BAD_DIGEST = unchecked((int)0x80096010);

        private const uint WTD_UI_NONE = 2;
        private const uint WTD_REVOKE_NONE = 0;
        private const uint WTD_CHOICE_FILE = 1;
        private const uint WTD_STATEACTION_IGNORE = 0;
        private const uint WTD_REVOCATION_CHECK_NONE = 0x10;
        private const uint WTD_CACHE_ONLY_URL_RETRIEVAL = 0x1000;
        private const uint WTD_DISABLE_MD2_MD4 = 0x2000;

        private static readonly Guid GenericVerifyV2 =
            new Guid("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");

        [StructLayout(LayoutKind.Sequential)]
        private struct WINTRUST_FILE_INFO
        {
            public uint cbStruct;
            public IntPtr pcwszFilePath;
            public IntPtr hFile;
            public IntPtr pgKnownSubject;
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct WINTRUST_DATA
        {
            public uint cbStruct;
            public IntPtr pPolicyCallbackData;
            public IntPtr pSIPClientData;
            public uint dwUIChoice;
            public uint fdwRevocationChecks;
            public uint dwUnionChoice;
            public IntPtr pFile;
            public uint dwStateAction;
            public IntPtr hWVTStateData;
            public IntPtr pwszURLReference;
            public uint dwProvFlags;
            public uint dwUIContext;
            public IntPtr pSignatureSettings;
        }

        [DllImport("wintrust.dll", ExactSpelling = true)]
        private static extern int WinVerifyTrust(
            IntPtr hwnd,
            [In] ref Guid pgActionID,
            ref WINTRUST_DATA pWVTData);

        public static int VerifyEmbeddedFile(string path)
        {
            if (String.IsNullOrWhiteSpace(path))
            {
                throw new ArgumentNullException(nameof(path));
            }

            path = Path.GetFullPath(path);
            if (!File.Exists(path))
            {
                throw new FileNotFoundException("Artifact not found.", path);
            }

            IntPtr pathPointer = IntPtr.Zero;
            IntPtr fileInfoPointer = IntPtr.Zero;
            try
            {
                pathPointer = Marshal.StringToHGlobalUni(path);
                var file = new WINTRUST_FILE_INFO
                {
                    cbStruct = (uint)Marshal.SizeOf<WINTRUST_FILE_INFO>(),
                    pcwszFilePath = pathPointer,
                    hFile = IntPtr.Zero,
                    pgKnownSubject = IntPtr.Zero
                };
                fileInfoPointer = Marshal.AllocHGlobal(Marshal.SizeOf<WINTRUST_FILE_INFO>());
                Marshal.StructureToPtr(file, fileInfoPointer, false);

                var data = new WINTRUST_DATA
                {
                    cbStruct = (uint)Marshal.SizeOf<WINTRUST_DATA>(),
                    dwUIChoice = WTD_UI_NONE,
                    fdwRevocationChecks = WTD_REVOKE_NONE,
                    dwUnionChoice = WTD_CHOICE_FILE,
                    pFile = fileInfoPointer,
                    dwStateAction = WTD_STATEACTION_IGNORE,
                    dwProvFlags =
                        WTD_REVOCATION_CHECK_NONE |
                        WTD_CACHE_ONLY_URL_RETRIEVAL |
                        WTD_DISABLE_MD2_MD4
                };
                Guid action = GenericVerifyV2;
                return WinVerifyTrust(new IntPtr(-1), ref action, ref data);
            }
            finally
            {
                if (fileInfoPointer != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(fileInfoPointer);
                }
                if (pathPointer != IntPtr.Zero)
                {
                    Marshal.FreeHGlobal(pathPointer);
                }
            }
        }
    }
}
'@
}

function Assert-ExternalVault {
    $resolvedVault = [System.IO.Path]::GetFullPath($VaultRoot)
    $rootPrefix = $root.TrimEnd('\') + '\'
    if ($resolvedVault.Equals($root, [System.StringComparison]::OrdinalIgnoreCase) -or
        $resolvedVault.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The signing Vault must be outside the public repository.'
    }
}

function Get-SignTool {
    $candidate = Get-ChildItem 'C:\Program Files (x86)\Windows Kits\10\bin' `
        -Recurse `
        -Filter 'signtool.exe' `
        -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -eq 'x64' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if ($null -eq $candidate) {
        throw 'SignTool.exe was not found. Install the Windows SDK signing tools.'
    }

    $signature = Get-AuthenticodeSignature -LiteralPath $candidate.FullName
    if ($signature.Status -ne [System.Management.Automation.SignatureStatus]::Valid -or
        $null -eq $signature.SignerCertificate -or
        $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
        throw "The Windows SDK SignTool does not have a valid Microsoft signature: $($candidate.FullName)"
    }
    $candidate.FullName
}

function Get-ExpectedThumbprint {
    if (-not [string]::IsNullOrWhiteSpace($CertificateThumbprint)) {
        $thumbprint = ($CertificateThumbprint -replace '\s', '').ToUpperInvariant()
    }
    elseif (Test-Path -LiteralPath $metadataPath -PathType Leaf) {
        $metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
        $thumbprint = ($metadata.thumbprint -replace '\s', '').ToUpperInvariant()
    }

    if (-not [string]::IsNullOrWhiteSpace($thumbprint) -and
        $thumbprint -notmatch '^[0-9A-F]{40}$') {
        throw 'The certificate thumbprint must contain exactly 40 hexadecimal characters.'
    }
    $thumbprint
}

function Test-ExpectedDevelopmentSigner {
    param([string]$ExpectedThumbprint)

    if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) {
        return $false
    }

    $metadata = Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
    $metadataThumbprint = ($metadata.thumbprint -replace '\s', '').ToUpperInvariant()
    $metadataThumbprint -eq $ExpectedThumbprint -and
        $metadata.purpose -eq 'Development code signing only' -and
        $metadata.trustedAutomatically -eq $false
}

function Get-WinTrustName {
    param([int]$Status)

    switch ($Status) {
        ([ViewsExplorer.Signing.AuthenticodeTrust]::S_OK) { 'S_OK'; break }
        ([ViewsExplorer.Signing.AuthenticodeTrust]::CERT_E_UNTRUSTEDROOT) {
            'CERT_E_UNTRUSTEDROOT'
            break
        }
        ([ViewsExplorer.Signing.AuthenticodeTrust]::TRUST_E_BAD_DIGEST) {
            'TRUST_E_BAD_DIGEST'
            break
        }
        ([ViewsExplorer.Signing.AuthenticodeTrust]::TRUST_E_NOSIGNATURE) {
            'TRUST_E_NOSIGNATURE'
            break
        }
        default { 'OTHER_TRUST_FAILURE' }
    }
}

function Get-StatusHex {
    param([int]$Status)

    $unsigned = [BitConverter]::ToUInt32([BitConverter]::GetBytes($Status), 0)
    '0x{0:X8}' -f $unsigned
}

function Get-SigningCertificate {
    $thumbprint = Get-ExpectedThumbprint
    if ([string]::IsNullOrWhiteSpace($thumbprint)) {
        throw 'No certificate thumbprint was supplied and no Vault metadata profile was found.'
    }

    $certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$thumbprint" -ErrorAction SilentlyContinue
    if ($null -eq $certificate) {
        throw "Certificate is not installed in CurrentUser\My: $thumbprint"
    }
    if (-not $certificate.HasPrivateKey) {
        throw "Certificate does not have an available private key: $thumbprint"
    }
    $now = Get-Date
    if ($now -lt $certificate.NotBefore -or $now -gt $certificate.NotAfter) {
        throw "Certificate is outside its validity period: $thumbprint"
    }

    $codeSigningOid = '1.3.6.1.5.5.7.3.3'
    if (-not ($certificate.Extensions |
        Where-Object { $_ -is [System.Security.Cryptography.X509Certificates.X509EnhancedKeyUsageExtension] } |
        ForEach-Object { $_.EnhancedKeyUsages } |
        Where-Object { $_.Value -eq $codeSigningOid })) {
        throw "Certificate is not valid for code signing: $thumbprint"
    }
    $certificate
}

function Get-ArtifactPaths {
    $requested = [System.Collections.Generic.List[string]]::new()
    if ($Path) {
        foreach ($item in $Path) {
            $requested.Add([System.IO.Path]::GetFullPath($item, $root))
        }
    }
    else {
        $architectures = if ($Architecture -eq 'All') { @('x64', 'ARM64') } else { @($Architecture) }
        foreach ($item in $architectures) {
            $requested.Add((Join-Path $root "src\ExplorerWelcome.NativeHost\out\$item\$Configuration\ExplorerWelcome.NativeHost.exe"))
            $requested.Add((Join-Path $root "src\ExplorerWelcome.NamespaceExtension\out\$item\$Configuration\ExplorerWelcome.NamespaceExtension.dll"))
        }
    }

    $rootPrefix = $root.TrimEnd('\') + '\'
    foreach ($item in $requested | Select-Object -Unique) {
        $resolved = [System.IO.Path]::GetFullPath($item)
        if (-not $resolved.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to sign a path outside the repository: $resolved"
        }
        if (-not (Test-Path -LiteralPath $resolved -PathType Leaf)) {
            throw "Artifact was not found: $resolved"
        }
        if ([System.IO.Path]::GetExtension($resolved) -notin $allowedExtensions) {
            throw "Unsupported signing artifact type: $resolved"
        }

        $current = Get-Item -LiteralPath $resolved -Force
        while ($true) {
            if (($current.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw "Refusing a signing path that contains a reparse point: $($current.FullName)"
            }
            if ($current.FullName.Equals($root, [System.StringComparison]::OrdinalIgnoreCase)) {
                break
            }
            $parentPath = if ($current -is [System.IO.FileInfo]) {
                $current.DirectoryName
            }
            else {
                $current.Parent.FullName
            }
            $current = Get-Item -LiteralPath $parentPath -Force
        }
        $resolved
    }
}

function Get-SignatureReport {
    param(
        [string[]]$ArtifactPaths,
        [string]$ExpectedThumbprint
    )

    foreach ($artifact in $ArtifactPaths) {
        $signature = Get-AuthenticodeSignature -LiteralPath $artifact
        $actualThumbprint = if ($null -ne $signature.SignerCertificate) {
            $signature.SignerCertificate.Thumbprint
        }
        else {
            $null
        }
        $isEmbedded = $signature.SignatureType.ToString() -eq 'Authenticode'
        $expectedSigner = -not [string]::IsNullOrWhiteSpace($ExpectedThumbprint) -and
            $actualThumbprint -eq $ExpectedThumbprint
        $winTrustStatus = [ViewsExplorer.Signing.AuthenticodeTrust]::VerifyEmbeddedFile($artifact)
        $allowDevelopmentRoot = $expectedSigner -and
            (Test-ExpectedDevelopmentSigner -ExpectedThumbprint $ExpectedThumbprint)
        $integrityAccepted =
            $isEmbedded -and
            $expectedSigner -and
            ($winTrustStatus -eq [ViewsExplorer.Signing.AuthenticodeTrust]::S_OK -or
                ($allowDevelopmentRoot -and
                    $winTrustStatus -eq
                        [ViewsExplorer.Signing.AuthenticodeTrust]::CERT_E_UNTRUSTEDROOT))

        [pscustomobject]@{
            File = [System.IO.Path]::GetRelativePath($root, $artifact)
            Signed = $null -ne $signature.SignerCertificate
            EmbeddedAuthenticode = $isEmbedded
            ExpectedSigner = $expectedSigner
            IntegrityAccepted = $integrityAccepted
            TrustStatus = $signature.Status
            WinTrustStatus = Get-WinTrustName -Status $winTrustStatus
            WinTrustCode = Get-StatusHex -Status $winTrustStatus
            Timestamped = $null -ne $signature.TimeStamperCertificate
            Thumbprint = $actualThumbprint
        }
    }
}

function Show-Status {
    $thumbprint = Get-ExpectedThumbprint
    $installed = -not [string]::IsNullOrWhiteSpace($thumbprint) -and
        (Test-Path -LiteralPath "Cert:\CurrentUser\My\$thumbprint")
    [pscustomobject]@{
        Profile = $ProfileName
        VaultMetadataPresent = Test-Path -LiteralPath $metadataPath -PathType Leaf
        Thumbprint = $thumbprint
        CertificateInstalled = $installed
        SignTool = Get-SignTool
        PasswordAcceptedByScript = $false
    } | Format-List
}

function Invoke-Sign {
    $signTool = Get-SignTool
    $certificate = Get-SigningCertificate
    $artifacts = @(Get-ArtifactPaths)

    foreach ($artifact in $artifacts) {
        if (-not $PSCmdlet.ShouldProcess($artifact, "Sign with $($certificate.Thumbprint)")) {
            continue
        }

        $arguments = @(
            'sign',
            '/fd', 'SHA256',
            '/sha1', $certificate.Thumbprint,
            '/s', 'My'
        )
        if (-not [string]::IsNullOrWhiteSpace($TimestampUrl)) {
            $arguments += @('/tr', $TimestampUrl, '/td', 'SHA256')
        }
        elseif ($RequireTimestamp) {
            throw 'A timestamp URL is required when -RequireTimestamp is set.'
        }
        $arguments += $artifact

        & $signTool @arguments | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "SignTool failed for $artifact with exit code $LASTEXITCODE."
        }
    }

    $report = @(Get-SignatureReport -ArtifactPaths $artifacts -ExpectedThumbprint $certificate.Thumbprint)
    $report | Format-List File, Signed, EmbeddedAuthenticode, ExpectedSigner, IntegrityAccepted, TrustStatus, WinTrustStatus, WinTrustCode, Timestamped, Thumbprint
    if ($report | Where-Object {
        -not $_.Signed -or -not $_.ExpectedSigner -or -not $_.IntegrityAccepted
    }) {
        throw 'One or more artifacts do not contain an acceptable expected signature.'
    }
    if ($RequireTimestamp -and ($report | Where-Object { -not $_.Timestamped })) {
        throw 'One or more artifacts do not contain a timestamp.'
    }
}

function Invoke-Verify {
    $artifacts = @(Get-ArtifactPaths)
    $thumbprint = Get-ExpectedThumbprint
    if ([string]::IsNullOrWhiteSpace($thumbprint)) {
        throw 'Verification requires an explicit certificate thumbprint or a valid external Vault profile.'
    }
    $report = @(Get-SignatureReport -ArtifactPaths $artifacts -ExpectedThumbprint $thumbprint)
    $report | Format-List File, Signed, EmbeddedAuthenticode, ExpectedSigner, IntegrityAccepted, TrustStatus, WinTrustStatus, WinTrustCode, Timestamped, Thumbprint
    if ($report | Where-Object {
        -not $_.Signed -or -not $_.ExpectedSigner -or -not $_.IntegrityAccepted
    }) {
        throw 'One or more artifacts are unsigned, damaged, or use an unexpected signer.'
    }
    if ($RequireTimestamp -and ($report | Where-Object { -not $_.Timestamped })) {
        throw 'One or more artifacts do not contain a timestamp.'
    }
}

function Invoke-Menu {
    while ($true) {
        Write-Host ''
        Write-Host 'Views Explorer Welcome POC - artifact signing'
        Write-Host '  1. Status'
        Write-Host '  2. Sign Release x64 and ARM64'
        Write-Host '  3. Verify Release x64 and ARM64'
        Write-Host '  0. Exit'
        $choice = Read-Host 'Select'
        switch ($choice) {
            '1' { Show-Status }
            '2' { Invoke-Sign }
            '3' { Invoke-Verify }
            '0' { return }
            default { Write-Warning 'Unsupported choice.' }
        }
    }
}

Assert-ExternalVault

switch ($Action) {
    'Menu' { Invoke-Menu }
    'Sign' { Invoke-Sign }
    'Verify' { Invoke-Verify }
    'Status' { Show-Status }
}
