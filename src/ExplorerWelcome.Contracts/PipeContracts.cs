// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Versioned named-pipe contract shared by the lightweight host and heavy app.
// -----------------------------------------------------------------------------
namespace ExplorerWelcome.Contracts;

public static class PipeProtocol
{
    public const int CurrentVersion = 1;
    public const string PipeName = "views-explorer-welcome-poc";
    public const int MaxLineLength = 16 * 1024;
}

public sealed record PipeRequest(int Version, string Type);

public sealed record DriveSnapshot(
    string Name,
    string? Label,
    long FreeBytes,
    long TotalBytes);

public sealed record WelcomePageSnapshot(
    int Version,
    DateTimeOffset GeneratedUtc,
    string Title,
    string Subtitle,
    IReadOnlyList<DriveSnapshot> Drives,
    IReadOnlyList<string> PinnedLocations);

public sealed record PipeResponse(
    int Version,
    string Type,
    WelcomePageSnapshot? Snapshot = null,
    string? Error = null);
