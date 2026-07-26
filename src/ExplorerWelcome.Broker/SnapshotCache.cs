// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Small recoverable current-user cache for the last valid dashboard snapshot.
// -----------------------------------------------------------------------------
using System.Text.Json;
using ExplorerWelcome.Contracts;

namespace ExplorerWelcome.Broker;

internal sealed class SnapshotCache
{
    private readonly JsonSerializerOptions _json = new(JsonSerializerDefaults.Web);
    private readonly string _path = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "ViewsExplorerWelcome",
        "snapshot-cache.json");

    public bool TryLoad(out WelcomePageSnapshot snapshot)
    {
        snapshot = new WelcomePageSnapshot();
        try
        {
            var file = new FileInfo(_path);
            if (!file.Exists || file.Length is <= 0 or > PipeProtocol.MaxLineLength)
            {
                return false;
            }

            var loaded = JsonSerializer.Deserialize<WelcomePageSnapshot>(
                File.ReadAllText(_path),
                _json);
            if (loaded is null || loaded.Version != PipeProtocol.CurrentVersion)
            {
                return false;
            }

            snapshot = loaded;
            return true;
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException or JsonException)
        {
            return false;
        }
    }

    public void TrySave(WelcomePageSnapshot snapshot)
    {
        try
        {
            var directory = Path.GetDirectoryName(_path)!;
            Directory.CreateDirectory(directory);
            var temporaryPath = _path + ".tmp";
            File.WriteAllText(temporaryPath, JsonSerializer.Serialize(snapshot, _json));
            File.Move(temporaryPath, _path, true);
        }
        catch (Exception exception) when (
            exception is IOException or UnauthorizedAccessException)
        {
            // Cache failure never makes a live snapshot request fail.
        }
    }
}
