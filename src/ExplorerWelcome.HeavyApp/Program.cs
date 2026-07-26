// =============================================================================
// Views Explorer Welcome POC
// File: src/ExplorerWelcome.HeavyApp/Program.cs
// Purpose: Separate heavy-app seam used to validate the named-pipe contract.
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
using ExplorerWelcome.Contracts;

await using var client = new NamedPipeClientStream(
    ".",
    PipeProtocol.PipeName,
    PipeDirection.InOut,
    PipeOptions.Asynchronous);

await client.ConnectAsync(2_000);
using var reader = new StreamReader(client);
await using var writer = new StreamWriter(client) { AutoFlush = true };
var json = new JsonSerializerOptions(JsonSerializerDefaults.Web);

await writer.WriteLineAsync(JsonSerializer.Serialize(
    new PipeRequest(PipeProtocol.CurrentVersion, PipeProtocol.SnapshotRequest, Guid.NewGuid().ToString("N")), json));

var line = await reader.ReadLineAsync();
var response = line is null ? null : JsonSerializer.Deserialize<PipeResponse>(line, json);
if (response?.Snapshot is null)
{
    Console.Error.WriteLine(response?.Error ?? "No snapshot response received.");
    return 1;
}

Console.WriteLine(response.Snapshot.Title);
Console.WriteLine($"Machine: {response.Snapshot.Machine.Name}");
Console.WriteLine($"Storage: {response.Snapshot.Storage.Count}");
Console.WriteLine($"Recent: {response.Snapshot.RecentItems.Count}");
Console.WriteLine($"Tools: {response.Snapshot.Tools.Count}");
return 0;
