// =============================================================================
// Views Explorer Welcome POC
// File: src/ExplorerWelcome.Broker/Program.cs
// Purpose: Out-of-process, current-user-only named-pipe broker for Explorer Home V2.
//
// Copyright (c) 2026 Umberto Giacobbi
// Author: Umberto Giacobbi
// Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
// License: PolyForm Noncommercial License 1.0.0
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Open source: https://umbertogiacobbi.biz/opensource
// =============================================================================

using System.IO.Pipes;
using System.Text.Json;
using ExplorerWelcome.Broker;
using ExplorerWelcome.Contracts;

var json = new JsonSerializerOptions(JsonSerializerDefaults.Web)
{
    MaxDepth = 16
};
var collector = new SnapshotCollector();
var preferencesStore = new PreferencesStore();
var snapshotCache = new SnapshotCache();

Console.WriteLine($"ExplorerWelcome.Broker listening on {PipeProtocol.PipeName}");

while (true)
{
    try
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

        using var readTimeout = new CancellationTokenSource(TimeSpan.FromSeconds(5));
        var boundedLine = await BoundedLineReader.ReadAsync(
            reader,
            PipeProtocol.MaxLineLength,
            readTimeout.Token);

        if (boundedLine.IsTooLong)
        {
            await WriteAsync(new PipeResponse(
                PipeProtocol.CurrentVersion,
                "error",
                "broker",
                Error: "Request is empty or too large."));
            continue;
        }

        var parsed = PipeRequestParser.Parse(boundedLine.Line, json);
        var correlationId = parsed.CorrelationId;
        if (parsed.Error is not null)
        {
            await WriteAsync(new PipeResponse(
                PipeProtocol.CurrentVersion,
                "error",
                correlationId,
                Error: parsed.Error));
            continue;
        }

        var request = parsed.Request!;

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
                    snapshotCache.TrySave(snapshot);
                    await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "snapshot.response", correlationId, Snapshot: snapshot));
                }
                catch (OperationCanceledException)
                {
                    await WriteStaleOrErrorAsync("Snapshot collection timed out.");
                }
                catch (Exception)
                {
                    await WriteStaleOrErrorAsync("Snapshot collection failed.");
                }
                break;

            case PipeProtocol.MetricsRequest:
                try
                {
                    // Live refreshes sample only fast counters and never touch the full snapshot cache.
                    using var timeout = new CancellationTokenSource(TimeSpan.FromMilliseconds(500));
                    var metrics = await collector.CollectMetricsAsync(timeout.Token);
                    await WriteAsync(new PipeResponse(
                        PipeProtocol.CurrentVersion,
                        "metrics.response",
                        correlationId,
                        Metrics: metrics));
                }
                catch (OperationCanceledException)
                {
                    await WriteAsync(new PipeResponse(
                        PipeProtocol.CurrentVersion,
                        "metrics.error",
                        correlationId,
                        Error: "Metric collection timed out."));
                }
                catch (Exception)
                {
                    await WriteAsync(new PipeResponse(
                        PipeProtocol.CurrentVersion,
                        "metrics.error",
                        correlationId,
                        Error: "Metric collection failed."));
                }
                break;

            case PipeProtocol.ActionRequest:
                var action = ActionLauncher.TryLaunch(request, preferencesStore.Load());
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

        async Task WriteStaleOrErrorAsync(string error)
        {
            if (snapshotCache.TryLoad(out var cached))
            {
                var stale = cached with
                {
                    Freshness = cached.Freshness with
                    {
                        BrokerAvailable = true,
                        IsStale = true,
                        SectionErrors = new Dictionary<string, string>(cached.Freshness.SectionErrors)
                        {
                            ["snapshot"] = error
                        }
                    }
                };
                await WriteAsync(new PipeResponse(
                    PipeProtocol.CurrentVersion,
                    "snapshot.response",
                    correlationId,
                    Snapshot: stale,
                    Error: error));
                return;
            }

            await WriteAsync(new PipeResponse(
                PipeProtocol.CurrentVersion,
                "snapshot.error",
                correlationId,
                Error: error));
        }
    }
    catch (OperationCanceledException)
    {
        // A connected client gets five seconds to send one bounded request.
    }
    catch (IOException)
    {
        // A disconnected local client must not terminate the long-running broker.
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
