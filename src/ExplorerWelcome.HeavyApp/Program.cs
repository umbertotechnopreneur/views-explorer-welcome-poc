// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Separate heavy-app seam used to validate the named-pipe contract.
// -----------------------------------------------------------------------------
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
    new PipeRequest(PipeProtocol.CurrentVersion, "snapshot.request"), json));

var line = await reader.ReadLineAsync();
var response = line is null ? null : JsonSerializer.Deserialize<PipeResponse>(line, json);
if (response?.Snapshot is null)
{
    Console.Error.WriteLine(response?.Error ?? "No snapshot response received.");
    return 1;
}

Console.WriteLine(response.Snapshot.Title);
Console.WriteLine(response.Snapshot.Subtitle);
Console.WriteLine($"Drives: {response.Snapshot.Drives.Count}");
return 0;
