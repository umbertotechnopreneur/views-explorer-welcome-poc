// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Out-of-process, current-user-only named-pipe broker for Explorer Home V2.
// -----------------------------------------------------------------------------
using System.Diagnostics;
using System.IO.Pipes;
using System.Text.Json;
using ExplorerWelcome.Broker;
using ExplorerWelcome.Contracts;

const string defaultCorrelationId = "broker";
var json = new JsonSerializerOptions(JsonSerializerDefaults.Web);
var collector = new SnapshotCollector();
var preferencesStore = new PreferencesStore();

Console.WriteLine($"ExplorerWelcome.Broker listening on {PipeProtocol.PipeName}");

while (true)
{
    await using var server = new NamedPipeServerStream(
        PipeProtocol.PipeName,
        PipeDirection.InOut,
        1,
        PipeTransmissionMode.Byte,
        PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);

    await server.WaitForConnectionAsync();
    using var reader = new StreamReader(server);
    await using var writer = new StreamWriter(server) { AutoFlush = true };

    var line = await reader.ReadLineAsync();
    var correlationId = defaultCorrelationId;
    if (string.IsNullOrWhiteSpace(line) || line.Length > PipeProtocol.MaxLineLength)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", correlationId, Error: "Request is empty or too large."));
        continue;
    }

    PipeRequest? request;
    try
    {
        request = JsonSerializer.Deserialize<PipeRequest>(line, json);
        correlationId = request?.CorrelationId ?? Guid.NewGuid().ToString("N");
    }
    catch (JsonException)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", correlationId, Error: "Request JSON is invalid."));
        continue;
    }

    if (request is null || request.Version != PipeProtocol.CurrentVersion)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", correlationId, Error: "Unsupported protocol version."));
        continue;
    }

    switch (request.Type)
    {
        case PipeProtocol.HostPingRequest:
            await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "host.pong", correlationId));
            break;

        case PipeProtocol.SnapshotRequest:
            try
            {
                using var timeout = new CancellationTokenSource(TimeSpan.FromMilliseconds(1_500));
                var snapshot = await collector.CollectAsync(preferencesStore.Load(), timeout.Token);
                await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "snapshot.response", correlationId, Snapshot: snapshot));
            }
            catch (OperationCanceledException)
            {
                await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "snapshot.stale", correlationId, Error: "Snapshot collection timed out."));
            }
            catch (Exception)
            {
                await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "snapshot.error", correlationId, Error: "Snapshot collection failed."));
            }
            break;

        case PipeProtocol.ActionRequest:
            var action = ActionLauncher.TryLaunch(request);
            await WriteAsync(new PipeResponse(
                PipeProtocol.CurrentVersion,
                action.Accepted ? "action.accepted" : "action.rejected",
                correlationId,
                ActionResult: action));
            break;

        case PipeProtocol.PreferencesRequest:
            var preferencesResult = TryUpdatePreferences(request, preferencesStore);
            await WriteAsync(new PipeResponse(
                PipeProtocol.CurrentVersion,
                preferencesResult.Accepted ? "preferences.accepted" : "preferences.rejected",
                correlationId,
                ActionResult: preferencesResult));
            break;

        default:
            await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", correlationId, Error: "Unsupported request type."));
            break;
    }

    async Task WriteAsync(PipeResponse response)
    {
        await writer.WriteLineAsync(JsonSerializer.Serialize(response, json));
    }
}

static ActionResult TryUpdatePreferences(PipeRequest request, PreferencesStore store)
{
    if (request.Arguments is null || request.Arguments.Count > 16)
    {
        return new ActionResult(false, PipeProtocol.PreferencesRequest, "Preferences payload is missing or too large.");
    }

    var current = store.Load();
    var updated = current;
    foreach (var pair in request.Arguments)
    {
        switch (pair.Key)
        {
            case "heroCollapsed" when bool.TryParse(pair.Value, out var collapsed):
                updated = updated with { HeroCollapsed = collapsed };
                break;
            case "folderNavigation" when pair.Value is "SameExplorerTab" or "NewExplorerWindow" or "FollowWindowsSetting":
                updated = updated with { FolderNavigation = pair.Value };
                break;
            case "metricRefreshMode" when pair.Value is "Live" or "Reduced" or "Paused":
                updated = updated with { MetricRefreshMode = pair.Value };
                break;
            case "defaultWorkingDirectory" when pair.Value.Length <= 4_096 && !pair.Value.Contains('\0'):
                updated = updated with { DefaultWorkingDirectory = pair.Value };
                break;
            default:
                return new ActionResult(false, PipeProtocol.PreferencesRequest, "Unsupported preference update.");
        }
    }

    return store.TrySave(updated, out var error)
        ? new ActionResult(true, PipeProtocol.PreferencesRequest, "Preferences saved.")
        : new ActionResult(false, PipeProtocol.PreferencesRequest, error);
}

internal static class ActionLauncher
{
    public static ActionResult TryLaunch(PipeRequest request)
    {
        if (string.IsNullOrWhiteSpace(request.Action))
        {
            return new ActionResult(false, "unknown", "Action is required.");
        }

        return request.Action switch
        {
            "settings.open" => LaunchSettings(request.Target),
            "folder.open" => LaunchFolder(request.Target),
            _ => new ActionResult(false, request.Action, "Unsupported action.")
        };
    }

    private static ActionResult LaunchSettings(string? target)
    {
        if (string.IsNullOrWhiteSpace(target) || !target.StartsWith("ms-settings:", StringComparison.OrdinalIgnoreCase) || target.Length > 256)
        {
            return new ActionResult(false, "settings.open", "Only documented ms-settings URIs are accepted.");
        }

        try
        {
            Process.Start(new ProcessStartInfo(target) { UseShellExecute = true });
            return new ActionResult(true, "settings.open");
        }
        catch
        {
            return new ActionResult(false, "settings.open", "Settings activation failed.");
        }
    }

    private static ActionResult LaunchFolder(string? target)
    {
        if (string.IsNullOrWhiteSpace(target) || target.Length > 4_096 || target.Contains('\0'))
        {
            return new ActionResult(false, "folder.open", "Folder target is invalid.");
        }

        try
        {
            var start = new ProcessStartInfo("explorer.exe")
            {
                UseShellExecute = false
            };
            start.ArgumentList.Add(target);
            Process.Start(start);
            return new ActionResult(true, "folder.open");
        }
        catch
        {
            return new ActionResult(false, "folder.open", "Explorer activation failed.");
        }
    }
}
