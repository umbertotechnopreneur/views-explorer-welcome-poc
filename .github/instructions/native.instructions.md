---
applyTo: "src/ExplorerWelcome.NativeHost/**/*"
---

# Native host instructions

- Use C++/WinRT and the system `DesktopWindowXamlSource` API for this first experiment.
- Keep XAML controls on `ElementTheme::Default` unless a test explicitly compares a theme override.
- Do not add file indexing, network access, AI calls, arbitrary command execution, or unbounded allocations to the native host.
- Keep architecture configurations explicit: `x64` and `ARM64`.
