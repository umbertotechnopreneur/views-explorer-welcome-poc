# Public publication checklist

- [ ] Run the repository sanitizer prompt and review redacted findings.
- [ ] Confirm no secrets exist in current files, ignored files, or reachable Git history.
- [ ] Confirm no PFX, DPAPI credential export, private certificate, or signing
  service token exists in the working tree or Git history.
- [ ] Audit dependencies, screenshots, fonts, icons, and generated assets.
- [ ] Confirm `LICENSE`, `NOTICE.md`, and `THIRD_PARTY_NOTICES.md` match the actual contents.
- [ ] Run x64 and ARM64 targeted builds.
- [ ] Run the broker/client named-pipe smoke test.
- [ ] Manually review the native XAML host behavior and accessibility boundary.
- [ ] Keep Explorer registration and elevation disabled until separately reviewed.
- [ ] Enable GitHub secret scanning and dependency alerts after repository creation.
- [ ] Before distribution, use an authorized production code-signing identity
  and RFC 3161 timestamp; never distribute artifacts signed only by the local
  self-signed development certificate.
