---
applyTo: "src/ExplorerWelcome.Contracts/**/*,src/ExplorerWelcome.Broker/**/*,src/ExplorerWelcome.HeavyApp/**/*"
---

# Protocol instructions

- Every message includes a protocol version and explicit type.
- Reject unknown versions and unsupported request types; do not silently downgrade.
- Keep messages bounded and line-delimited for the first POC.
- Keep the named pipe restricted to the current user and avoid logging payloads that may contain private data.
