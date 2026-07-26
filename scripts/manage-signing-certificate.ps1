<#
===============================================================================
Views Explorer Welcome POC
File: scripts/manage-signing-certificate.ps1
Purpose: Manage the local development code-signing certificate without exposing secrets.

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
  Manage the local development code-signing certificate without exposing secrets.

.DESCRIPTION
  Creates or imports a self-signed development certificate. The password-
  protected PFX and its DPAPI-protected PSCredential remain outside Git in the
  encrypted workstation Vault. No action trusts the certificate.

.PARAMETER Action
  Menu opens the interactive menu. Create generates a new certificate and Vault
  profile. Import imports the existing Vault PFX into CurrentUser\My. Status
  reports non-secret metadata only.

.EXAMPLE
  pwsh -NoProfile -File .\scripts\manage-signing-certificate.ps1 -Action Status

.EXAMPLE
  pwsh -NoProfile -File .\scripts\manage-signing-certificate.ps1 -Action Create -Confirm:$false
#>
[CmdletBinding(SupportsShouldProcess, ConfirmImpact = 'High')]
param(
    [ValidateSet('Menu', 'Create', 'Import', 'Status')]
    [string]$Action = 'Menu',
    [string]$VaultRoot = (Join-Path $env:USERPROFILE 'OneDrive\Obsidian\Vault'),
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9 ._-]{2,100}$')]
    [string]$ProfileName = 'Views Explorer Welcome POC Development',
    [ValidateRange(1, 36)]
    [int]$ValidityMonths = 24,
    [switch]$Help
)

if ($Help) { Get-Help $PSCommandPath -Detailed; exit 0 }
$ErrorActionPreference = 'Stop'

$profileDirectory = Join-Path $VaultRoot 'Views Explorer Welcome POC\Signing'
$pfxPath = Join-Path $profileDirectory "$ProfileName.pfx"
$credentialPath = Join-Path $profileDirectory "$ProfileName.credential.xml"
$publicCertificatePath = Join-Path $profileDirectory "$ProfileName.cer"
$metadataPath = Join-Path $profileDirectory "$ProfileName.metadata.json"
$storePath = 'Cert:\CurrentUser\My'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Assert-ExternalVault {
    $resolvedVault = [System.IO.Path]::GetFullPath($VaultRoot)
    $rootPrefix = $repoRoot.TrimEnd('\') + '\'
    if ($resolvedVault.Equals($repoRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
        $resolvedVault.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The signing Vault must be outside the public repository.'
    }
}

function Get-ProfileMetadata {
    if (-not (Test-Path -LiteralPath $metadataPath -PathType Leaf)) {
        return $null
    }

    Get-Content -Raw -LiteralPath $metadataPath | ConvertFrom-Json
}

function Get-InstalledCertificate {
    param([string]$Thumbprint)

    if ([string]::IsNullOrWhiteSpace($Thumbprint)) {
        return $null
    }

    Get-Item -LiteralPath (Join-Path $storePath $Thumbprint) -ErrorAction SilentlyContinue
}

function Get-KeyExportPolicy {
    param(
        [System.Security.Cryptography.X509Certificates.X509Certificate2]
        $Certificate
    )

    if ($null -eq $Certificate -or -not $Certificate.HasPrivateKey) {
        return 'NoPrivateKey'
    }

    $key = $null
    try {
        $key = [System.Security.Cryptography.X509Certificates.RSACertificateExtensions]::GetRSAPrivateKey(
            $Certificate)
        if ($key -is [System.Security.Cryptography.RSACng]) {
            return $key.Key.ExportPolicy.ToString()
        }
        'UnknownProvider'
    }
    finally {
        if ($null -ne $key) {
            $key.Dispose()
        }
    }
}

function Show-Status {
    $metadata = Get-ProfileMetadata
    if ($null -eq $metadata) {
        [pscustomobject]@{
            Profile = $ProfileName
            VaultProfilePresent = $false
            CertificateInstalled = $false
            TrustedAutomatically = $false
        } | Format-List
        return
    }

    $certificate = Get-InstalledCertificate -Thumbprint $metadata.thumbprint
    [pscustomobject]@{
        Profile = $metadata.profile
        Subject = $metadata.subject
        Thumbprint = $metadata.thumbprint
        CreatedUtc = $metadata.createdUtc
        ExpiresUtc = $metadata.expiresUtc
        VaultProfilePresent =
            (Test-Path -LiteralPath $pfxPath -PathType Leaf) -and
            (Test-Path -LiteralPath $credentialPath -PathType Leaf)
        CertificateInstalled = $null -ne $certificate
        HasPrivateKey = $null -ne $certificate -and $certificate.HasPrivateKey
        KeyExportPolicy = Get-KeyExportPolicy -Certificate $certificate
        TrustedAutomatically = $false
    } | Format-List
}

function New-DevelopmentCertificate {
    foreach ($path in @($pfxPath, $credentialPath, $publicCertificatePath, $metadataPath)) {
        if (Test-Path -LiteralPath $path) {
            throw "Refusing to overwrite an existing signing profile file: $path"
        }
    }

    if (-not $PSCmdlet.ShouldProcess(
        $profileDirectory,
        'Create a self-signed development certificate and DPAPI-protected Vault profile')) {
        return
    }

    New-Item -ItemType Directory -Path $profileDirectory -Force | Out-Null
    $certificate = $null
    $createdThumbprint = $null
    $passwordBytes = $null
    try {
        $subject = "CN=$ProfileName"
        $certificate = New-SelfSignedCertificate `
            -Type CodeSigningCert `
            -Subject $subject `
            -FriendlyName $ProfileName `
            -CertStoreLocation $storePath `
            -KeyAlgorithm RSA `
            -KeyLength 3072 `
            -HashAlgorithm SHA256 `
            -KeyExportPolicy Exportable `
            -KeyUsage DigitalSignature `
            -NotAfter (Get-Date).AddMonths($ValidityMonths)
        $createdThumbprint = $certificate.Thumbprint
        $passwordBytes = [System.Security.Cryptography.RandomNumberGenerator]::GetBytes(48)
        $transientText = [Convert]::ToBase64String($passwordBytes)
        $pfxProtection = ConvertTo-SecureString -String $transientText -AsPlainText -Force
        $credential = [pscredential]::new('ViewsExplorerWelcomePfx', $pfxProtection)

        $credential | Export-Clixml -LiteralPath $credentialPath
        Export-PfxCertificate `
            -Cert $certificate `
            -FilePath $pfxPath `
            -Password $pfxProtection `
            -CryptoAlgorithmOption AES256_SHA256 | Out-Null
        Export-Certificate `
            -Cert $certificate `
            -FilePath $publicCertificatePath `
            -Type CERT | Out-Null

        [ordered]@{
            schemaVersion = 1
            profile = $ProfileName
            purpose = 'Development code signing only'
            subject = $certificate.Subject
            thumbprint = $certificate.Thumbprint
            createdUtc = (Get-Date).ToUniversalTime().ToString('o')
            expiresUtc = $certificate.NotAfter.ToUniversalTime().ToString('o')
            storeLocation = 'CurrentUser\My'
            pfxFile = [System.IO.Path]::GetFileName($pfxPath)
            credentialFile = [System.IO.Path]::GetFileName($credentialPath)
            publicCertificateFile = [System.IO.Path]::GetFileName($publicCertificatePath)
            trustedAutomatically = $false
        } | ConvertTo-Json | Set-Content -LiteralPath $metadataPath -Encoding utf8NoBOM

        # The initial key must be exportable to create the protected Vault PFX.
        # Replace only that new store copy with a non-exportable private key.
        Remove-Item -LiteralPath (Join-Path $storePath $certificate.Thumbprint)
        $imported = Import-PfxCertificate `
            -FilePath $pfxPath `
            -CertStoreLocation $storePath `
            -Password $pfxProtection `
            -Exportable:$false
        if ($imported.Thumbprint -ne $certificate.Thumbprint) {
            throw 'The non-exportable store copy does not match the created certificate.'
        }
        $certificate = $imported
        if ((Get-KeyExportPolicy -Certificate $certificate) -ne 'None') {
            throw 'The certificate-store private key is still exportable.'
        }
    }
    catch {
        if (-not [string]::IsNullOrWhiteSpace($createdThumbprint)) {
            Remove-Item `
                -LiteralPath (Join-Path $storePath $createdThumbprint) `
                -Force `
                -ErrorAction SilentlyContinue
        }
        foreach ($path in @($pfxPath, $credentialPath, $publicCertificatePath, $metadataPath)) {
            Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
        }
        throw
    }
    finally {
        if ($null -ne $passwordBytes) {
            [System.Security.Cryptography.CryptographicOperations]::ZeroMemory($passwordBytes)
        }
        $transientText = $null
        $pfxProtection = $null
        $credential = $null
    }

    Write-Output "Development signing profile created under: $profileDirectory"
    Write-Output "Certificate thumbprint: $($certificate.Thumbprint)"
    Write-Output "Store key export policy: $(Get-KeyExportPolicy -Certificate $certificate)"
    Write-Output 'The private-key password was not printed or copied to the clipboard.'
    Write-Output 'The self-signed certificate was not added to a trusted store.'
}

function Import-DevelopmentCertificate {
    $metadata = Get-ProfileMetadata
    if ($null -eq $metadata) {
        throw "Signing metadata was not found: $metadataPath"
    }
    if (-not (Test-Path -LiteralPath $pfxPath -PathType Leaf)) {
        throw "Signing PFX was not found: $pfxPath"
    }
    if (-not (Test-Path -LiteralPath $credentialPath -PathType Leaf)) {
        throw "DPAPI credential was not found: $credentialPath"
    }

    $existing = Get-InstalledCertificate -Thumbprint $metadata.thumbprint
    $replaceExportable = $false
    if ($null -ne $existing -and $existing.HasPrivateKey) {
        $exportPolicy = Get-KeyExportPolicy -Certificate $existing
        if ($exportPolicy -eq 'None') {
            Write-Output "Non-exportable certificate is already installed in CurrentUser\My: $($metadata.thumbprint)"
            return
        }
        $replaceExportable = $true
    }

    $operation = if ($replaceExportable) {
        "Replace exportable store copy $($metadata.thumbprint) with a non-exportable copy from the encrypted Vault"
    }
    else {
        "Import development signing certificate $($metadata.thumbprint) from the encrypted Vault"
    }
    if (-not $PSCmdlet.ShouldProcess(
        $storePath,
        $operation)) {
        return
    }

    $credential = Import-Clixml -LiteralPath $credentialPath
    if ($credential -isnot [pscredential]) {
        throw 'The DPAPI signing credential is not a PSCredential.'
    }

    $probe = $null
    $removedExisting = $false
    try {
        $probe = [System.Security.Cryptography.X509Certificates.X509Certificate2]::new(
            $pfxPath,
            $credential.Password,
            [System.Security.Cryptography.X509Certificates.X509KeyStorageFlags]::EphemeralKeySet)
        if ($probe.Thumbprint -ne $metadata.thumbprint -or -not $probe.HasPrivateKey) {
            throw 'The Vault PFX does not match the metadata or has no private key.'
        }

        if ($replaceExportable) {
            Remove-Item -LiteralPath (Join-Path $storePath $metadata.thumbprint)
            $removedExisting = $true
        }

        try {
            $importedCertificates = @(Import-PfxCertificate `
                -FilePath $pfxPath `
                -CertStoreLocation $storePath `
                -Password $credential.Password `
                -Exportable:$false)
            $imported = $importedCertificates |
                Where-Object { $_.Thumbprint -eq $metadata.thumbprint } |
                Select-Object -First 1
            if ($null -eq $imported -or
                -not $imported.HasPrivateKey -or
                (Get-KeyExportPolicy -Certificate $imported) -ne 'None') {
                throw 'The imported certificate is missing or its private key is exportable.'
            }
        }
        catch {
            if ($removedExisting) {
                Remove-Item `
                    -LiteralPath (Join-Path $storePath $metadata.thumbprint) `
                    -Force `
                    -ErrorAction SilentlyContinue
                Import-PfxCertificate `
                    -FilePath $pfxPath `
                    -CertStoreLocation $storePath `
                    -Password $credential.Password `
                    -Exportable:$true | Out-Null
            }
            throw
        }
    }
    finally {
        if ($null -ne $probe) {
            $probe.Dispose()
        }
        $credential = $null
    }

    Write-Output "Certificate imported into CurrentUser\My: $($imported.Thumbprint)"
    Write-Output 'No certificate trust was added.'
}

function Invoke-Menu {
    while ($true) {
        Write-Host ''
        Write-Host 'Views Explorer Welcome POC - development signing certificate'
        Write-Host '  1. Status'
        Write-Host '  2. Create a new Vault profile'
        Write-Host '  3. Import the Vault certificate'
        Write-Host '  0. Exit'
        $choice = Read-Host 'Select'
        switch ($choice) {
            '1' { Show-Status }
            '2' { New-DevelopmentCertificate }
            '3' { Import-DevelopmentCertificate }
            '0' { return }
            default { Write-Warning 'Unsupported choice.' }
        }
    }
}

Assert-ExternalVault

switch ($Action) {
    'Menu' { Invoke-Menu }
    'Create' { New-DevelopmentCertificate }
    'Import' { Import-DevelopmentCertificate }
    'Status' { Show-Status }
}
