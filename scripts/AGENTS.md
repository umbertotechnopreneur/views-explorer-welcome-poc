# Script rules

- Every example and invocation uses `pwsh -NoProfile`.
- Keep checks explicit, bounded, and safe by default.
- Build scripts may fail fast but must not delete user files or rewrite Git history.
- Secret scans report paths and categories only; never print matched values.
