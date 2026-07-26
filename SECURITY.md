# Security policy

## Scope

This is an early Windows feasibility study. Do not use it as a production Explorer extension or deploy it with elevated privileges.

## Reporting

Please do not file public issues containing credentials, private endpoints, crash dumps, or personal data. Contact the copyright holder privately through the GitHub profile associated with this repository and include a minimal reproduction without secrets.

## Design rules

- The named pipe is current-user-only and uses a bounded, versioned protocol.
- The native host must not accept arbitrary file paths, shell commands, or code from the pipe.
- Heavy work remains out of the future Explorer call path.
- Never commit API keys, passwords, certificates, tokens, or local configuration.
- Keep signing PFX files and DPAPI credential exports in the encrypted external
  Vault, never in this working tree.
- Artifact signing selects a certificate from the Windows user certificate
  store by public thumbprint. Do not pass PFX passwords on command lines or in
  environment variables.
- A self-signed development certificate must not be added automatically to
  `Root` or `TrustedPublisher`.
