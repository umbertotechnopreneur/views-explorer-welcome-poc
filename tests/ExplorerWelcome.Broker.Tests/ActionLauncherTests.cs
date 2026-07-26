using ExplorerWelcome.Broker;
using ExplorerWelcome.Contracts;
using Xunit;

namespace ExplorerWelcome.Broker.Tests;

public sealed class ActionLauncherTests
{
    private static readonly PreferencesSnapshot Preferences = new();

    [Fact]
    public void MissingActionIsRejected()
    {
        var request = Request(action: null, target: null);

        var result = ActionLauncher.TryLaunch(request, Preferences);

        Assert.False(result.Accepted);
        Assert.Equal("unknown", result.Action);
    }

    [Fact]
    public void UnknownActionIsRejected()
    {
        var request = Request("process.start", "cmd.exe");

        var result = ActionLauncher.TryLaunch(request, Preferences);

        Assert.False(result.Accepted);
        Assert.Equal("process.start", result.Action);
    }

    [Theory]
    [InlineData("ms-settings:privacy")]
    [InlineData("https://example.com")]
    [InlineData("cmd.exe")]
    public void SettingsOutsideCatalogAreRejected(string target)
    {
        var result = ActionLauncher.TryLaunch(Request("settings.open", target), Preferences);

        Assert.False(result.Accepted);
    }

    [Fact]
    public void MissingFolderIsRejected()
    {
        var missing = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N"));

        var result = ActionLauncher.TryLaunch(Request("folder.open", missing), Preferences);

        Assert.False(result.Accepted);
    }

    [Fact]
    public void MissingRecentItemIsRejected()
    {
        var missing = Path.Combine(Path.GetTempPath(), Guid.NewGuid().ToString("N") + ".txt");

        var result = ActionLauncher.TryLaunch(Request("item.open", missing), Preferences);

        Assert.False(result.Accepted);
    }

    [Theory]
    [InlineData("powershell -EncodedCommand AAAA")]
    [InlineData("cmd.exe /c whoami")]
    [InlineData("../powershell")]
    public void TerminalProfileMustBeAnExactCatalogId(string profileId)
    {
        var result = ActionLauncher.TryLaunch(Request("terminal.open", profileId), Preferences);

        Assert.False(result.Accepted);
    }

    private static PipeRequest Request(string? action, string? target)
        => new(
            PipeProtocol.CurrentVersion,
            PipeProtocol.ActionRequest,
            Guid.NewGuid().ToString("N"),
            action,
            target);
}
