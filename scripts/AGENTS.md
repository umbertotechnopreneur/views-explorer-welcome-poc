# Script rules

- Every example and invocation uses `pwsh -NoProfile`.
- Keep checks explicit, bounded, and safe by default.
- Build scripts may fail fast but must not delete user files or rewrite Git history.
- Secret scans report paths and categories only; never print matched values.
- `manage-source-banners.ps1` is the repository-local source-header authority.
  Keep it standalone, previewable, reversible, encoding-preserving, and safe
  for non-interactive CI use; do not add a dependency on workstation assets.
