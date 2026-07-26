# Branding and editorial voice

This guide keeps public documentation for Views Explorer Welcome POC
consistent, precise, and honest about the experiment's boundaries.

## Canonical identity

- Project name: **Views Explorer Welcome POC**
- Short descriptive name: **Explorer Welcome POC**
- Publisher and author: **Umberto Giacobbi**
- Public open-source reference:
  **https://umbertogiacobbi.biz/opensource**
- Repository status: **public feasibility study**
- License description: **source-available for noncommercial use under the
  PolyForm Noncommercial License 1.0.0**

Do not shorten the product name to `Views` where it could be confused with the
separate Views product. Use the full project name at first mention.

## Voice

- Lead with the user outcome, then state the technical mechanism.
- Prefer short declarative sentences and concrete evidence.
- Distinguish implemented behavior, validated behavior, planned work, and
  unsupported claims.
- Write for a technical reader who understands Windows development but may not
  know Shell extension terminology.
- Treat safety boundaries as part of the design, not as a disclaimer added at
  the end.
- Use restrained language. Prefer `explores`, `demonstrates`, `validated in the
  standalone host`, and `controlled experiment` over promotional superlatives.

## Preferred terminology

| Prefer | Avoid | Reason |
| --- | --- | --- |
| Windows 11-quality welcome surface | Windows replacement | The project augments a scoped surface |
| Explorer-facing native boundary | Explorer add-in or plugin | Namespace Extension is the precise Shell model |
| native Win32/C++/WinRT XAML Island | embedded WinUI app | The phrase states the actual hosting boundary |
| out-of-process broker | backend inside Explorer | Heavy work must stay outside `explorer.exe` |
| versioned current-user named pipe | local API | The transport and trust boundary matter |
| controlled Namespace Extension experiment | production integration | Production readiness is not established |
| theme-aware system XAML controls | custom Windows theme | Windows remains the theme source of truth |
| x64 and ARM64 | cross-platform | The supported targets are Windows architectures |

Context-menu extension, shell command injection, arbitrary process launching,
and MSIX delivery of the in-process Explorer DLL are outside project scope.

## Claim levels

Use one of these forms when documenting evidence:

- **Implemented:** available in the current source tree.
- **Automated validation:** covered by a named build, test, or safety check.
- **Manual validation:** observed on a stated Windows configuration.
- **Feasibility hypothesis:** requires a controlled Explorer test.
- **Production requirement:** necessary before distribution but not yet
  delivered.

Never turn a successful standalone XAML host test into a claim of safe
production behavior inside Explorer.

## Reusable README wording

### Opening

> Views Explorer Welcome POC is a public feasibility study for a Windows
> 11-quality welcome surface in File Explorer. It explores a thin native
> Explorer-facing boundary while snapshots, orchestration, and other heavier
> work remain in an out-of-process broker.

### Safety boundary

> The default workflow does not register a component in Explorer. Namespace
> Extension registration is an explicit, reversible experiment and should be
> used only on a development machine after both architecture-specific builds
> and the repository safety checks pass.

### Architecture summary

> The native component owns Shell integration and theme-aware presentation
> only. The broker owns slow or failure-prone work behind a bounded, versioned,
> current-user named-pipe contract.

### Project status

> This repository demonstrates feasibility; it does not claim production
> readiness. Installer servicing, public-trust signing, update and rollback,
> accessibility validation, and long-running Explorer stability require
> separate evidence.

### License

> Source code is available under the PolyForm Noncommercial License 1.0.0.
> Commercial use requires separate written authorization from the copyright
> holder. Licensing and open-source information is available at
> https://umbertogiacobbi.biz/opensource.

### AI assistance

> AI tools may assist with drafting, implementation, tests, and documentation.
> Human review remains responsible for correctness, provenance, security,
> licensing, validation, and publication decisions.

## Documentation structure

Use this order when creating a new major public document:

1. Purpose and scope.
2. Current status or decision.
3. Architecture and trust boundaries.
4. Reproduction or development workflow.
5. Validation evidence.
6. Known limitations and next experiment.
7. Security, provenance, license, and AI-assistance references when applicable.

## Review checklist

- The canonical project name appears at first mention.
- Explorer integration is described as a controlled feasibility experiment.
- Native and out-of-process responsibilities remain distinct.
- x64 and ARM64 claims match the actual validation evidence.
- Commands use `pwsh -NoProfile`.
- No private paths, endpoints, credentials, certificate material, or personal
  workstation details appear.
- License wording matches the root `LICENSE`.
- Public repository links use canonical HTTPS URLs.
- The open-source reference is present:
  https://umbertogiacobbi.biz/opensource.
