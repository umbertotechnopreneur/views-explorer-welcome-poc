// -----------------------------------------------------------------------------
// Views Explorer Welcome POC
// Bounded, best-effort Windows snapshot collection for Explorer Home V2.
// -----------------------------------------------------------------------------
using Microsoft.Win32;
using System.Diagnostics;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

using ExplorerWelcome.Contracts;

namespace ExplorerWelcome.Broker;

[SupportedOSPlatform("windows")]
public sealed class SnapshotCollector
{
    private readonly List<double> _cpuSamples = new();
    private readonly List<double> _networkSamples = new();

    public async Task<WelcomePageSnapshot> CollectAsync(PreferencesSnapshot preferences, CancellationToken cancellationToken)
    {
        var errors = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var beforeCpu = ReadSystemTimes();
        var beforeNetwork = ReadNetworkTotals();

        // Keep the first sample bounded while giving the rate calculator a useful interval.
        await Task.Delay(TimeSpan.FromMilliseconds(100), cancellationToken);

        var afterCpu = ReadSystemTimes();
        var afterNetwork = ReadNetworkTotals();
        var metrics = BuildMetrics(beforeCpu, afterCpu, beforeNetwork, afterNetwork, errors);

        var machine = CollectMachine(preferences, errors);
        var storage = CollectStorage(errors);
        var networkLocations = CollectNetworkLocations(errors);
        var recentItems = CollectRecentItems(errors);
        var tools = ToolDiscovery.Discover(errors);
        var terminalProfiles = tools
            .Where(tool => tool.SupportedTargetKinds.Contains("Folder", StringComparer.OrdinalIgnoreCase))
            .Select(tool => new TerminalProfileSnapshot
            {
                Id = tool.Id,
                DisplayName = tool.DisplayName,
                Executable = tool.Executable,
                IsAvailable = tool.IsEnabled,
                Version = tool.Version,
                SupportedTargetKinds = tool.SupportedTargetKinds
            })
            .ToArray();

        var storagePercent = storage
            .Where(item => item.TotalBytes is > 0 && item.FreeBytes is >= 0)
            .Select(item => 100d * (item.TotalBytes!.Value - item.FreeBytes!.Value) / item.TotalBytes.Value)
            .FirstOrDefault();

        metrics = metrics with { StoragePercent = storagePercent > 0 ? storagePercent : null };
        if (machine.GpuModel == "Unavailable")
        {
            errors.TryAdd("gpu", "GPU identity and utilization are not available from the current broker collector.");
        }

        if (recentItems.Count == 0)
        {
            errors.TryAdd("recentItems", "Windows did not expose any recent items.");
        }

        errors.TryAdd("highlightedItems", "Supported Windows-backed pin enumeration is not proven; no private pin store is used.");

        return new WelcomePageSnapshot
        {
            Version = PipeProtocol.CurrentVersion,
            GeneratedUtc = DateTimeOffset.UtcNow,
            Machine = machine,
            Metrics = metrics,
            Storage = storage,
            NetworkLocations = networkLocations,
            RecentItems = recentItems,
            HighlightedItems = Array.Empty<HighlightedItemSnapshot>(),
            TerminalProfiles = terminalProfiles,
            Tools = tools,
            QuickSettings = QuickSettingsCatalog.Defaults,
            Preferences = preferences,
            Freshness = new FreshnessSnapshot
            {
                GeneratedUtc = DateTimeOffset.UtcNow,
                BrokerAvailable = true,
                IsStale = false,
                SectionErrors = errors
            }
        };
    }

    private MetricsSnapshot BuildMetrics(
        SystemTimes? beforeCpu,
        SystemTimes? afterCpu,
        NetworkTotals? beforeNetwork,
        NetworkTotals? afterNetwork,
        IDictionary<string, string> errors)
    {
        var cpuPercent = CalculateCpuPercent(beforeCpu, afterCpu);
        var sendRate = CalculateRate(beforeNetwork?.BytesSent, afterNetwork?.BytesSent);
        var receiveRate = CalculateRate(beforeNetwork?.BytesReceived, afterNetwork?.BytesReceived);

        if (cpuPercent is null) errors.TryAdd("cpu", "CPU utilization is not available from the current broker collector.");
        if (sendRate is null || receiveRate is null) errors.TryAdd("networkMetrics", "Network rate is not available from the current broker collector.");

        if (cpuPercent is not null)
        {
            _cpuSamples.Add(cpuPercent.Value);
            TrimSamples(_cpuSamples);
        }

        if (sendRate is not null && receiveRate is not null)
        {
            _networkSamples.Add(Math.Min(100d, (sendRate.Value + receiveRate.Value) / 1_000_000d));
            TrimSamples(_networkSamples);
        }

        return new MetricsSnapshot
        {
            CpuPercent = cpuPercent,
            MemoryPercent = ReadMemoryPercent(),
            NetworkSendBytesPerSecond = sendRate,
            NetworkReceiveBytesPerSecond = receiveRate,
            CpuSparkline = _cpuSamples.ToArray(),
            NetworkSparkline = _networkSamples.ToArray(),
            SampledUtc = DateTimeOffset.UtcNow
        };
    }

    private static MachineSnapshot CollectMachine(PreferencesSnapshot preferences, IDictionary<string, string> errors)
    {
        var memory = ReadMemory();
        var primaryAddress = preferences.PrivacyShowNetworkIdentity ? ReadPrimaryNetworkAddress() : null;
        var model = ReadRegistryString(
            RegistryHive.LocalMachine,
            @"SOFTWARE\Microsoft\Windows NT\CurrentVersion",
            "ProductName") ?? "Unknown";
        var deviceModel = ReadRegistryString(
            RegistryHive.LocalMachine,
            @"SYSTEM\CurrentControlSet\Control\SystemInformation",
            "SystemProductName") ?? model;
        var cpu = ReadRegistryString(
            RegistryHive.LocalMachine,
            @"HARDWARE\DESCRIPTION\System\CentralProcessor\0",
            "ProcessorNameString") ?? "Unknown";
        var displayVersion = ReadRegistryString(
            RegistryHive.LocalMachine,
            @"SOFTWARE\Microsoft\Windows NT\CurrentVersion",
            "DisplayVersion") ?? Environment.OSVersion.Version.ToString();
        var wallpaper = ReadRegistryString(
            RegistryHive.CurrentUser,
            @"Control Panel\Desktop",
            "WallPaper");

        if (!preferences.PrivacyShowDeviceDetails)
        {
            deviceModel = "Hidden by privacy preference";
            cpu = "Hidden by privacy preference";
        }

        return new MachineSnapshot
        {
            Name = Environment.MachineName,
            Model = deviceModel,
            OsDisplayVersion = displayVersion,
            PrimaryNetworkIdentity = primaryAddress,
            CpuModel = cpu,
            GpuModel = "Unavailable",
            MemoryUsedBytes = memory.UsedBytes,
            MemoryTotalBytes = memory.TotalBytes,
            WallpaperPath = string.IsNullOrWhiteSpace(wallpaper) ? null : wallpaper
        };
    }

    private static IReadOnlyList<StorageSnapshot> CollectStorage(IDictionary<string, string> errors)
    {
        var items = new List<StorageSnapshot>();
        try
        {
            foreach (var drive in DriveInfo.GetDrives().Take(PipeProtocol.MaxCollectionCount))
            {
                try
                {
                    var isReady = drive.IsReady;
                    items.Add(new StorageSnapshot
                    {
                        Id = drive.Name,
                        Path = drive.RootDirectory.FullName,
                        Label = isReady && !string.IsNullOrWhiteSpace(drive.VolumeLabel) ? drive.VolumeLabel : null,
                        Kind = drive.DriveType.ToString(),
                        FreeBytes = isReady ? drive.AvailableFreeSpace : null,
                        TotalBytes = isReady ? drive.TotalSize : null,
                        FileSystem = isReady ? drive.DriveFormat : null,
                        Health = "Unknown",
                        IsReady = isReady,
                        IsRemovable = drive.DriveType is DriveType.Removable or DriveType.CDRom,
                        IsNetwork = drive.DriveType == DriveType.Network,
                        Error = isReady ? "Health is not exposed by this collector." : "Drive is not ready."
                    });
                }
                catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                {
                    items.Add(new StorageSnapshot
                    {
                        Id = drive.Name,
                        Path = drive.Name,
                        Kind = drive.DriveType.ToString(),
                        Health = "Unknown",
                        IsReady = false,
                        Error = exception.Message
                    });
                }
            }
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            errors.TryAdd("storage", exception.Message);
        }

        return items;
    }

    private static IReadOnlyList<NetworkLocationSnapshot> CollectNetworkLocations(IDictionary<string, string> errors)
    {
        var locations = new List<NetworkLocationSnapshot>();
        try
        {
            foreach (var drive in DriveInfo.GetDrives().Where(item => item.DriveType == DriveType.Network).Take(PipeProtocol.MaxCollectionCount))
            {
                var ready = false;
                long? free = null;
                long? total = null;
                string? error = null;
                try
                {
                    ready = drive.IsReady;
                    if (ready)
                    {
                        free = drive.AvailableFreeSpace;
                        total = drive.TotalSize;
                    }
                }
                catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
                {
                    error = exception.Message;
                }

                locations.Add(new NetworkLocationSnapshot
                {
                    Id = drive.Name,
                    Path = drive.Name,
                    DisplayName = string.IsNullOrWhiteSpace(drive.VolumeLabel) ? drive.Name : drive.VolumeLabel,
                    State = ready ? "Connected" : "Unavailable",
                    FreeBytes = free,
                    TotalBytes = total,
                    Error = error
                });
            }
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            errors.TryAdd("networkLocations", exception.Message);
        }

        return locations;
    }

    private static IReadOnlyList<RecentItemSnapshot> CollectRecentItems(IDictionary<string, string> errors)
    {
        try
        {
            var recentRoot = Environment.GetFolderPath(Environment.SpecialFolder.Recent);
            if (string.IsNullOrWhiteSpace(recentRoot) || !Directory.Exists(recentRoot))
            {
                return Array.Empty<RecentItemSnapshot>();
            }

            return Directory.EnumerateFiles(recentRoot)
                .Select(path => new FileInfo(path))
                .OrderByDescending(file => file.LastWriteTimeUtc)
                .Take(5)
                .Select(file => new RecentItemSnapshot
                {
                    Id = file.FullName,
                    DisplayName = Path.GetFileNameWithoutExtension(file.Name),
                    Parent = recentRoot,
                    TargetKind = Path.GetExtension(file.Name).Equals(".lnk", StringComparison.OrdinalIgnoreCase) ? "ShellLink" : "File",
                    TimestampUtc = file.LastWriteTimeUtc,
                    IsAvailable = true
                })
                .ToArray();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            errors.TryAdd("recentItems", exception.Message);
            return Array.Empty<RecentItemSnapshot>();
        }
    }

    private static double? CalculateCpuPercent(SystemTimes? before, SystemTimes? after)
    {
        if (before is null || after is null) return null;
        var totalDelta = after.Value.Total - before.Value.Total;
        var idleDelta = after.Value.Idle - before.Value.Idle;
        if (totalDelta <= 0) return null;
        return Math.Clamp(100d * (totalDelta - idleDelta) / totalDelta, 0d, 100d);
    }

    private static long? CalculateRate(long? before, long? after)
    {
        if (before is null || after is null || after < before) return null;
        return Math.Max(0, (after.Value - before.Value) * 10);
    }

    private static double? ReadMemoryPercent()
    {
        var memory = ReadMemory();
        return memory.TotalBytes > 0 ? Math.Clamp(100d * memory.UsedBytes / memory.TotalBytes, 0d, 100d) : null;
    }

    private static (long UsedBytes, long TotalBytes) ReadMemory()
    {
        var status = new MemoryStatusEx { Length = (uint)Marshal.SizeOf<MemoryStatusEx>() };
        return GlobalMemoryStatusEx(ref status)
            ? ((long)status.TotalPhysical - (long)status.AvailablePhysical, (long)status.TotalPhysical)
            : (0, 0);
    }

    private static string? ReadPrimaryNetworkAddress()
    {
        try
        {
            var addresses = NetworkInterface.GetAllNetworkInterfaces()
                .Where(network => network.OperationalStatus == OperationalStatus.Up)
                .SelectMany(network => network.GetIPProperties().UnicastAddresses)
                .Select(item => item.Address)
                .Where(address => !IPAddress.IsLoopback(address) && address.AddressFamily is System.Net.Sockets.AddressFamily.InterNetwork or System.Net.Sockets.AddressFamily.InterNetworkV6)
                .OrderBy(address => address.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork ? 0 : 1)
                .FirstOrDefault();
            return addresses?.ToString();
        }
        catch (NetworkInformationException)
        {
            return null;
        }
    }

    private static string? ReadRegistryString(RegistryHive hive, string subKey, string valueName)
    {
        try
        {
            using var key = RegistryKey.OpenBaseKey(hive, RegistryView.Default).OpenSubKey(subKey);
            return key?.GetValue(valueName)?.ToString();
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            return null;
        }
    }

    private static SystemTimes? ReadSystemTimes()
    {
        if (!GetSystemTimes(out var idle, out var kernel, out var user)) return null;
        return new SystemTimes(ToUInt64(idle), ToUInt64(kernel), ToUInt64(user));
    }

    private static NetworkTotals? ReadNetworkTotals()
    {
        try
        {
            long sent = 0;
            long received = 0;
            foreach (var network in NetworkInterface.GetAllNetworkInterfaces())
            {
                if (network.OperationalStatus != OperationalStatus.Up) continue;
                var statistics = network.GetIPv4Statistics();
                sent += statistics.BytesSent;
                received += statistics.BytesReceived;
            }
            return new NetworkTotals(sent, received);
        }
        catch (NetworkInformationException)
        {
            return null;
        }
    }

    private static ulong ToUInt64(System.Runtime.InteropServices.ComTypes.FILETIME value)
        => ((ulong)(uint)value.dwHighDateTime << 32) | (uint)value.dwLowDateTime;

    private static void TrimSamples(List<double> samples)
    {
        while (samples.Count > 24) samples.RemoveAt(0);
    }

    private readonly record struct SystemTimes(ulong Idle, ulong Kernel, ulong User)
    {
        public ulong Total => Kernel + User;
    }

    private readonly record struct NetworkTotals(long BytesSent, long BytesReceived);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct MemoryStatusEx
    {
        public uint Length;
        public uint MemoryLoad;
        public ulong TotalPhysical;
        public ulong AvailablePhysical;
        public ulong TotalPageFile;
        public ulong AvailablePageFile;
        public ulong TotalVirtual;
        public ulong AvailableVirtual;
        public ulong AvailableExtendedVirtual;
    }

    [DllImport("kernel32.dll")]
    private static extern bool GlobalMemoryStatusEx(ref MemoryStatusEx buffer);

    [DllImport("kernel32.dll")]
    private static extern bool GetSystemTimes(
        out System.Runtime.InteropServices.ComTypes.FILETIME idleTime,
        out System.Runtime.InteropServices.ComTypes.FILETIME kernelTime,
        out System.Runtime.InteropServices.ComTypes.FILETIME userTime);
}

internal static class QuickSettingsCatalog
{
    public static IReadOnlyList<QuickSettingSnapshot> Defaults { get; } = new[]
    {
        new QuickSettingSnapshot { Id = "display", DisplayName = "Display", Uri = "ms-settings:display" },
        new QuickSettingSnapshot { Id = "sound", DisplayName = "Sound", Uri = "ms-settings:sound" },
        new QuickSettingSnapshot { Id = "network", DisplayName = "Network and Internet", Uri = "ms-settings:network" },
        new QuickSettingSnapshot { Id = "bluetooth", DisplayName = "Bluetooth and devices", Uri = "ms-settings:bluetooth" },
        new QuickSettingSnapshot { Id = "storage", DisplayName = "Storage", Uri = "ms-settings:storagesense" },
        new QuickSettingSnapshot { Id = "update", DisplayName = "Windows Update", Uri = "ms-settings:windowsupdate" },
        new QuickSettingSnapshot { Id = "personalization", DisplayName = "Personalization", Uri = "ms-settings:personalization" },
        new QuickSettingSnapshot { Id = "apps", DisplayName = "Apps", Uri = "ms-settings:appsfeatures" }
    };
}

internal static class ToolDiscovery
{
    private static readonly (string Id, string DisplayName, string[] Names, string[] Kinds)[] Catalog =
    {
        ("windows-terminal", "Windows Terminal", new[] { "wt.exe" }, new[] { "Folder", "Drive", "Network" }),
        ("powershell", "PowerShell 7", new[] { "pwsh.exe" }, new[] { "Folder", "Drive" }),
        ("command-prompt", "Prompt dei comandi", new[] { "cmd.exe" }, new[] { "Folder", "Drive" }),
        ("wsl", "Ubuntu (WSL)", new[] { "wsl.exe" }, new[] { "Folder" }),
        ("vscode", "Visual Studio Code", new[] { "code.exe", "code.cmd" }, new[] { "File", "Folder", "Workspace" }),
        ("visual-studio", "Visual Studio", new[] { "devenv.exe" }, new[] { "Solution", "Project", "Folder" }),
        ("github-cli", "GitHub CLI", new[] { "gh.exe" }, new[] { "Repository", "Url" }),
        ("codex", "Codex", new[] { "codex.exe" }, new[] { "Repository", "Folder" })
    };

    public static IReadOnlyList<ToolSnapshot> Discover(IDictionary<string, string> errors)
    {
        var result = new List<ToolSnapshot>();
        foreach (var adapter in Catalog)
        {
            var executable = FindOnPath(adapter.Names);
            result.Add(new ToolSnapshot
            {
                Id = adapter.Id,
                DisplayName = adapter.DisplayName,
                Executable = executable ?? adapter.Names[0],
                IsEnabled = executable is not null,
                SupportedTargetKinds = adapter.Kinds
            });
        }
        return result.Where(tool => tool.IsEnabled).ToArray();
    }

    private static string? FindOnPath(IEnumerable<string> names)
    {
        var path = Environment.GetEnvironmentVariable("PATH") ?? string.Empty;
        foreach (var directory in path.Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
        {
            foreach (var name in names)
            {
                var candidate = Path.Combine(directory, name);
                if (File.Exists(candidate)) return candidate;
            }
        }
        return null;
    }
}
