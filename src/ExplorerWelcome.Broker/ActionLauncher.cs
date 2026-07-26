// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Strict, identifier-based activation adapters executed outside Explorer.
// -----------------------------------------------------------------------------
using System.Diagnostics;
using ExplorerWelcome.Contracts;

namespace ExplorerWelcome.Broker;

internal static class ActionLauncher
{
    private static readonly HashSet<string> TerminalIds = new(StringComparer.OrdinalIgnoreCase)
    {
        "windows-terminal",
        "powershell",
        "command-prompt",
        "wsl"
    };

    public static ActionResult TryLaunch(PipeRequest request, PreferencesSnapshot preferences)
    {
        if (string.IsNullOrWhiteSpace(request.Action))
        {
            return new ActionResult(false, "unknown", "Action is required.");
        }

        return request.Action switch
        {
            "settings.open" => LaunchSettings(request.Target),
            "folder.open" => LaunchFolder(request.Target),
            "item.open" => LaunchItem(request.Target),
            "terminal.open" => LaunchTerminal(request.Target, request.Arguments, preferences),
            _ => new ActionResult(false, request.Action, "Unsupported action.")
        };
    }

    private static ActionResult LaunchSettings(string? target)
    {
        var supported = QuickSettingsCatalog.Defaults.Any(setting =>
            string.Equals(setting.Uri, target, StringComparison.OrdinalIgnoreCase));
        if (!supported)
        {
            return new ActionResult(false, "settings.open", "Settings target is not in the supported catalog.");
        }

        return StartShellTarget(target!, "settings.open", "Settings activation failed.");
    }

    private static ActionResult LaunchFolder(string? target)
    {
        if (!IsBoundedPath(target) || !Directory.Exists(target))
        {
            return new ActionResult(false, "folder.open", "Folder target is unavailable.");
        }

        try
        {
            var start = new ProcessStartInfo("explorer.exe")
            {
                UseShellExecute = false
            };
            start.ArgumentList.Add(target!);
            Process.Start(start);
            return new ActionResult(true, "folder.open");
        }
        catch
        {
            return new ActionResult(false, "folder.open", "Explorer activation failed.");
        }
    }

    private static ActionResult LaunchItem(string? target)
    {
        if (!IsBoundedPath(target) || (!File.Exists(target) && !Directory.Exists(target)))
        {
            return new ActionResult(false, "item.open", "Recent item is unavailable.");
        }

        return StartShellTarget(target!, "item.open", "Item activation failed.");
    }

    private static ActionResult LaunchTerminal(
        string? profileId,
        IReadOnlyDictionary<string, string>? arguments,
        PreferencesSnapshot preferences)
    {
        if (string.IsNullOrWhiteSpace(profileId) || !TerminalIds.Contains(profileId))
        {
            return new ActionResult(false, "terminal.open", "Terminal profile is not supported.");
        }

        var errors = new Dictionary<string, string>();
        var profile = ToolDiscovery.Discover(errors).FirstOrDefault(tool =>
            TerminalIds.Contains(tool.Id) &&
            string.Equals(tool.Id, profileId, StringComparison.OrdinalIgnoreCase));
        if (profile is null || !profile.IsEnabled || !File.Exists(profile.Executable))
        {
            return new ActionResult(false, "terminal.open", "Terminal executable is unavailable.");
        }

        var workingDirectory = ResolveWorkingDirectory(arguments, preferences);
        try
        {
            var start = new ProcessStartInfo(profile.Executable)
            {
                UseShellExecute = false,
                WorkingDirectory = workingDirectory
            };

            switch (profile.Id)
            {
                case "windows-terminal":
                    start.ArgumentList.Add("-d");
                    start.ArgumentList.Add(workingDirectory);
                    break;
                case "powershell":
                    start.ArgumentList.Add("-NoProfile");
                    start.ArgumentList.Add("-NoExit");
                    start.ArgumentList.Add("-WorkingDirectory");
                    start.ArgumentList.Add(workingDirectory);
                    break;
            }

            Process.Start(start);
            return new ActionResult(true, "terminal.open");
        }
        catch
        {
            return new ActionResult(false, "terminal.open", "Terminal activation failed.");
        }
    }

    private static string ResolveWorkingDirectory(
        IReadOnlyDictionary<string, string>? arguments,
        PreferencesSnapshot preferences)
    {
        var candidate = arguments is not null &&
            arguments.TryGetValue("workingDirectory", out var requested)
                ? requested
                : preferences.DefaultWorkingDirectory;

        if (IsBoundedPath(candidate) && Directory.Exists(candidate))
        {
            return Path.GetFullPath(candidate);
        }

        return Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    }

    private static bool IsBoundedPath(string? target)
        => !string.IsNullOrWhiteSpace(target) &&
           target.Length <= 4_096 &&
           !target.Contains('\0');

    private static ActionResult StartShellTarget(string target, string action, string error)
    {
        try
        {
            Process.Start(new ProcessStartInfo(target) { UseShellExecute = true });
            return new ActionResult(true, action);
        }
        catch
        {
            return new ActionResult(false, action, error);
        }
    }
}
