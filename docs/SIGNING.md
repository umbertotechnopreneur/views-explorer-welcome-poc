# Signing and secret handling

## Boundary

This public repository never stores private keys, PFX files, passwords,
DPAPI credential exports, tokens, or production signing configuration.

The canonical local workstation Vault is:

```text
%USERPROFILE%\OneDrive\Obsidian\Vault
```

The scripts use this project-specific profile directory by default:

```text
Views Explorer Welcome POC\Signing
```

The default development profile contains:

- a password-protected PFX;
- a `PSCredential` exported with `Export-Clixml`;
- a public CER;
- non-secret JSON metadata containing the certificate thumbprint.

The credential XML is protected by Windows DPAPI for the Windows user and
machine that created it. Encrypting or synchronizing the outer Vault does not
make that DPAPI file portable to another Windows account or machine. Re-wrap
the PFX password separately on each authorized workstation.

## Development certificate

Inspect status without importing a secret:

```powershell
pwsh -NoProfile -File .\scripts\manage-signing-certificate.ps1 -Action Status
```

Create a self-signed development certificate and Vault profile:

```powershell
pwsh -NoProfile -File .\scripts\manage-signing-certificate.ps1 `
  -Action Create `
  -Confirm:$false
```

On the same Windows user and machine, import an existing Vault profile:

```powershell
pwsh -NoProfile -File .\scripts\manage-signing-certificate.ps1 `
  -Action Import `
  -Confirm:$false
```

Neither action adds the self-signed certificate to `Root` or
`TrustedPublisher`. Development signatures therefore prove artifact identity
and integrity but are not a public trust chain.

The initial key is exportable only long enough to create the protected Vault
PFX. The manager then replaces that exact `CurrentUser\My` store copy with a
non-exportable private key. `-Action Status` reports the observed export policy.

## Sign local artifacts

Build both architectures first, then sign from `CurrentUser\My` by public
thumbprint:

```powershell
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture x64 -Configuration Release
pwsh -NoProfile -File .\scripts\build.ps1 -Architecture ARM64 -Configuration Release
pwsh -NoProfile -File .\scripts\sign-artifacts.ps1 `
  -Action Sign `
  -Architecture All `
  -Configuration Release
pwsh -NoProfile -File .\scripts\sign-artifacts.ps1 `
  -Action Verify `
  -Architecture All `
  -Configuration Release
```

The signing script deliberately has no password parameter. It refuses files
outside the repository, rejects reparse-point paths, and signs only approved
executable/package extensions. It uses the Microsoft-signed SignTool from the
Windows SDK rather than accepting a same-named executable from `PATH`.

Verification requires an exact 40-hexadecimal signer thumbprint and an embedded
Authenticode signature. Native `WinVerifyTrust` validates the payload. `S_OK`
is accepted for a trusted certificate; `CERT_E_UNTRUSTEDROOT` is accepted only
when the exact signer matches the explicit development profile in the external
Vault. Missing signatures, bad digests, unknown signers, catalog-only
signatures, and every other trust result fail closed.

For production signing, provide an authorized public code-signing certificate
or managed signing identity and require an RFC 3161 SHA-256 timestamp:

```powershell
pwsh -NoProfile -File .\scripts\sign-artifacts.ps1 `
  -Action Sign `
  -Path .\artifacts\installer\ViewsExplorerWelcome-x64.msi `
  -CertificateThumbprint '<PUBLIC-THUMBPRINT>' `
  -TimestampUrl '<AUTHORIZED-RFC3161-URL>' `
  -RequireTimestamp
```

Do not put a PFX password in a command line, environment variable, repository
file, log, issue, or CI output. CI production signing should use the selected
managed signing service or an environment secret store with short-lived access,
not this workstation DPAPI profile.

`validate-signing-boundary.ps1` scans tracked, untracked, and ignored files for
private signing material. It also checks that the signer has no secret-bearing
parameter or credential/PFX command.
