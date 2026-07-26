// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Out-of-process, current-user-only named-pipe broker.
// -----------------------------------------------------------------------------
using System.IO.Pipes;
using System.Text.Json;
using ExplorerWelcome.Contracts;

const string snapshotRequest = "snapshot.request";
const string hostPingRequest = "host.ping";
var json = new JsonSerializerOptions(JsonSerializerDefaults.Web);

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
    if (string.IsNullOrWhiteSpace(line) || line.Length > PipeProtocol.MaxLineLength)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", Error: "Request is empty or too large."));
        continue;
    }

    PipeRequest? request;
    try
    {
        request = JsonSerializer.Deserialize<PipeRequest>(line, json);
    }
    catch (JsonException)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", Error: "Request JSON is invalid."));
        continue;
    }

    if (request is null || request.Version != PipeProtocol.CurrentVersion)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", Error: "Unsupported protocol version."));
        continue;
    }

    if (request.Type == hostPingRequest)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "host.pong"));
        continue;
    }

    if (request.Type != snapshotRequest)
    {
        await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "error", Error: "Unsupported request type."));
        continue;
    }

    var drives = DriveInfo.GetDrives()
        .Where(drive => drive.IsReady)
        .Select(drive => new DriveSnapshot(
            drive.Name,
            string.IsNullOrWhiteSpace(drive.VolumeLabel) ? null : drive.VolumeLabel,
            drive.AvailableFreeSpace,
            drive.TotalSize))
        .ToArray();

    var snapshot = new WelcomePageSnapshot(
        PipeProtocol.CurrentVersion,
        DateTimeOffset.UtcNow,
        "Welcome to Views",
        "A calm place to start in File Explorer.",
        drives,
        new[] { "Desktop", "Documents", "Downloads" });

    await WriteAsync(new PipeResponse(PipeProtocol.CurrentVersion, "snapshot.response", snapshot));

    async Task WriteAsync(PipeResponse response)
    {
        await writer.WriteLineAsync(JsonSerializer.Serialize(response, json));
    }
}
