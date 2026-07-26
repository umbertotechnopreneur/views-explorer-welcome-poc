// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Current-user, versioned, recoverable preferences for Explorer Home V2.
// -----------------------------------------------------------------------------
using System.Text.Json;
using ExplorerWelcome.Contracts;

namespace ExplorerWelcome.Broker;

public sealed class PreferencesStore
{
    private readonly string _path = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "ViewsExplorerWelcome",
        "preferences.json");

    private readonly JsonSerializerOptions _json = new(JsonSerializerDefaults.Web)
    {
        WriteIndented = true
    };

    public PreferencesSnapshot Load()
    {
        try
        {
            if (!File.Exists(_path)) return new PreferencesSnapshot();
            var json = File.ReadAllText(_path);
            return JsonSerializer.Deserialize<PreferencesSnapshot>(json, _json) ?? new PreferencesSnapshot();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException)
        {
            return new PreferencesSnapshot();
        }
    }

    public bool TrySave(PreferencesSnapshot preferences, out string? error)
    {
        error = null;
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(_path)!);
            var temporaryPath = _path + ".tmp";
            File.WriteAllText(temporaryPath, JsonSerializer.Serialize(preferences, _json));
            File.Move(temporaryPath, _path, overwrite: true);
            return true;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            error = "Preferences could not be saved.";
            return false;
        }
    }
}
