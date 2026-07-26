# Packaging feasibility

## Result

The Explorer-facing DLL cannot be delivered as an MSIX in-process COM server.
Microsoft's MSIX preparation guidance explicitly excludes modules that must be
loaded into a process outside the package, including in-process Shell
extensions. `ExplorerWelcome.NamespaceExtension.dll` must therefore remain an
unpackaged, architecture-matched component installed and registered by a
classic installer.

The `identity` directory is intentionally narrower. It contains a package with
external location template that can grant identity to the standalone native
host or future out-of-process broker application. It does **not** register the
Namespace Extension and must never be described as doing so.

## Supported deployment boundary

| Component | Deployment direction |
| --- | --- |
| Namespace Extension DLL | Classic per-user or per-machine installer with reversible COM and Shell namespace registration |
| Broker and heavy app | Normal unpackaged deployment; optional package identity with external location |
| Standalone native host | Normal unpackaged deployment; optional package identity with external location |
| Context-menu integration | Excluded from this project |

An installer must choose the DLL matching Explorer's architecture, register the
CLSID and `This PC` namespace junction, start the broker outside Explorer, and
remove every owned registration during uninstall. The current PowerShell
registration script remains a development tool, not the production installer.

Signing is a separate boundary. Development PFX and DPAPI credential files stay
in the encrypted external Vault and are never installer inputs committed to
Git. Build artifacts are signed from the Windows certificate store by public
thumbprint. See [the signing guide](../docs/SIGNING.md).

## Build the identity-only package

The template uses placeholder development identity values and is unsigned. It
is suitable only for schema/build validation:

```powershell
pwsh -NoProfile -File .\scripts\build-identity-package.ps1
```

The generated package is written under the ignored `artifacts\packaging`
directory. The script does not sign, trust, install, register, or remove a
package. Before any real deployment, replace the publisher with the subject of
the production signing certificate and align the native executable's
side-by-side manifest with the same package name, publisher, and application
ID.

## Official references

- [Prepare to package a desktop application](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-prepare)
- [Grant package identity with external location manually](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/grant-identity-to-nonpackaged-apps)
- [Understanding Shell Namespace Extensions](https://learn.microsoft.com/en-us/windows/win32/shell/nse-works)
