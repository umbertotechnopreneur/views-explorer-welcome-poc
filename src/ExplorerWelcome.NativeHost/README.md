# ExplorerWelcome.NativeHost

This is the lightweight native experiment. It creates a Win32 top-level host, initializes `WindowsXamlManager`, attaches a `DesktopWindowXamlSource`, and renders a small card using system XAML controls with `ElementTheme::Default`.

It is deliberately not registered as an Explorer extension yet. That keeps the experiment reversible while we measure lifetime, rendering, input, DPI, accessibility, and theme behavior.
