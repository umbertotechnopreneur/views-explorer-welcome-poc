// =============================================================================
// Views Explorer Welcome POC
// File: src/ExplorerWelcome.Broker/SnapshotCollector.cs
// Purpose: Bounded, best-effort Windows snapshot collection for Explorer Home V2.
//
// Copyright (c) 2026 Umberto Giacobbi
// Author: Umberto Giacobbi
// Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
// License: PolyForm Noncommercial License 1.0.0
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Open source: https://umbertogiacobbi.biz/opensource
// =============================================================================

using Microsoft.Win32;
using System.Diagnostics;
using System.Net;
using System.Net.NetworkInformation;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Text;

using ExplorerWelcome.Contracts;

namespace ExplorerWelcome.Broker;

[SupportedOSPlatform("windows")]
public sealed class SnapshotCollector
{
    private readonly List<double> _cpuSamples = new();
    private readonly List<double> _networkSamples = new();

    // Collects only live metrics so the UI can refresh without rebuilding the full welcome snapshot.
    public Task<MetricsSnapshot> CollectMetricsAsync(CancellationToken cancellationToken)
    {
        var errors = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        return CollectMetricsAsync(errors, cancellationToken);
    }

    // Builds the complete welcome snapshot while retaining metric collection errors in freshness details.
    public async Task<WelcomePageSnapshot> CollectAsync(PreferencesSnapshot preferences, CancellationToken cancellationToken)
    {
        var errors = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        var metrics = await CollectMetricsAsync(errors, cancellationToken);

        var machine = CollectMachine(preferences, errors);
        var storage = CollectStorage(errors);
        var networkLocations = CollectNetworkLocations(errors);
        var recentItems = CollectRecentItems(errors);
        var tools = ToolDiscovery.Discover(errors);
        var terminalProfiles = tools
            .Where(tool => tool.Id is "windows-terminal" or "powershell" or "command-prompt" or "wsl")
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

        errors.TryAdd(
            "highlightedItems",
            "Elementi in evidenza non disponibili: l'enumerazione Windows supportata non è ancora implementata.");

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

    // Samples CPU and network counters over one bounded interval and updates their real history.
    private async Task<MetricsSnapshot> CollectMetricsAsync(
        IDictionary<string, string> errors,
        CancellationToken cancellationToken)
    {
        var beforeCpu = ReadSystemTimes();
        var beforeNetwork = ReadNetworkTotals();

        // Keep the first sample bounded while giving the rate calculator a useful interval.
        await Task.Delay(TimeSpan.FromMilliseconds(100), cancellationToken);

        var afterCpu = ReadSystemTimes();
        var afterNetwork = ReadNetworkTotals();
        return BuildMetrics(beforeCpu, afterCpu, beforeNetwork, afterNetwork, errors);
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
        var wallpaper = ReadDesktopWallpaper();
        if (string.IsNullOrWhiteSpace(wallpaper))
        {
            errors.TryAdd("wallpaper", "Windows did not expose a wallpaper image; the UI fallback is used.");
        }

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

    // Enumerates every mapped drive independently so one disconnected share cannot empty the section.
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
                string? label = null;
                string? error = null;
                var remotePath = ReadMappedNetworkPath(drive.Name);
                try
                {
                    ready = drive.IsReady;
                    if (ready)
                    {
                        label = drive.VolumeLabel;
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
                    DisplayName = FormatNetworkDisplayName(drive.Name, label, remotePath),
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

    // Reads the public per-user mapping metadata even when Windows reports the share disconnected.
    private static string? ReadMappedNetworkPath(string driveName)
    {
        if (string.IsNullOrWhiteSpace(driveName) || !char.IsAsciiLetter(driveName[0]))
        {
            return null;
        }

        try
        {
            using var key = Registry.CurrentUser.OpenSubKey($@"Network\{char.ToUpperInvariant(driveName[0])}");
            return key?.GetValue("RemotePath") as string;
        }
        catch (Exception exception) when (exception is UnauthorizedAccessException or System.Security.SecurityException)
        {
            return null;
        }
    }

    // Formats mapped shares as a readable share-and-host label without inventing connectivity.
    private static string FormatNetworkDisplayName(
        string driveName,
        string? volumeLabel,
        string? remotePath)
    {
        if (string.IsNullOrWhiteSpace(remotePath))
        {
            return string.IsNullOrWhiteSpace(volumeLabel) ? driveName : volumeLabel;
        }

        var parts = remotePath
            .Trim('\\')
            .Split('\\', StringSplitOptions.RemoveEmptyEntries);
        var displayName = string.IsNullOrWhiteSpace(volumeLabel)
            ? parts.LastOrDefault() ?? driveName
            : volumeLabel;
        return parts.Length >= 2
            ? $"{displayName} (\\\\{parts[0]})"
            : displayName;
    }

    // Reads Windows' Recent shell links and keeps only live, unique targets.
    private static IReadOnlyList<RecentItemSnapshot> CollectRecentItems(IDictionary<string, string> errors)
    {
        try
        {
            var recentRoot = Environment.GetFolderPath(Environment.SpecialFolder.Recent);
            if (string.IsNullOrWhiteSpace(recentRoot) || !Directory.Exists(recentRoot))
            {
                return Array.Empty<RecentItemSnapshot>();
            }

            var result = new List<RecentItemSnapshot>();
            var identities = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach (var file in Directory.EnumerateFiles(recentRoot)
                         .Select(path => new FileInfo(path))
                         .OrderByDescending(file => file.LastWriteTimeUtc)
                         .Take(32))
            {
                var target = file.Extension.Equals(".lnk", StringComparison.OrdinalIgnoreCase)
                    ? ReadShellLinkTarget(file.FullName)
                    : file.FullName;
                if (string.IsNullOrWhiteSpace(target))
                {
                    continue;
                }

                var isFile = File.Exists(target);
                var isDirectory = !isFile && Directory.Exists(target);
                if (!isFile && !isDirectory)
                {
                    continue;
                }

                var identity = Path.GetFullPath(target);
                if (!identities.Add(identity))
                {
                    continue;
                }

                result.Add(new RecentItemSnapshot
                {
                    Id = identity,
                    DisplayName = Path.GetFileName(target.TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)),
                    Parent = Path.GetDirectoryName(target),
                    TargetPath = target,
                    TargetKind = isDirectory ? "Folder" : "File",
                    TimestampUtc = file.LastWriteTimeUtc,
                    IsAvailable = true
                });

                if (result.Count == 5)
                {
                    break;
                }
            }

            return result;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            errors.TryAdd("recentItems", exception.Message);
            return Array.Empty<RecentItemSnapshot>();
        }
    }

    private static string? ReadDesktopWallpaper()
    {
        IDesktopWallpaper? wallpaper = null;
        try
        {
            var wallpaperType = Type.GetTypeFromCLSID(
                new Guid("C2CF3110-460E-4FC1-B9D0-8A1C0C9CC4BD"),
                throwOnError: true);
            wallpaper = (IDesktopWallpaper)Activator.CreateInstance(wallpaperType!)!;
            var path = wallpaper.GetWallpaper(null);
            if (!string.IsNullOrWhiteSpace(path))
            {
                return path;
            }

            if (wallpaper.GetMonitorDevicePathCount() > 0)
            {
                var monitor = wallpaper.GetMonitorDevicePathAt(0);
                path = wallpaper.GetWallpaper(monitor);
            }
            return string.IsNullOrWhiteSpace(path) ? null : path;
        }
        catch (COMException)
        {
            return null;
        }
        finally
        {
            if (wallpaper is not null)
            {
                Marshal.FinalReleaseComObject(wallpaper);
            }
        }
    }

    private static string? ReadShellLinkTarget(string shortcutPath)
    {
        IShellLinkW? link = null;
        try
        {
            var shellLinkType = Type.GetTypeFromCLSID(
                new Guid("00021401-0000-0000-C000-000000000046"),
                throwOnError: true);
            link = (IShellLinkW)Activator.CreateInstance(shellLinkType!)!;
            ((System.Runtime.InteropServices.ComTypes.IPersistFile)link).Load(shortcutPath, 0);
            var path = new StringBuilder(32768);
            link.GetPath(path, path.Capacity, out _, 4);
            return path.Length == 0 ? null : path.ToString();
        }
        catch (COMException)
        {
            return null;
        }
        finally
        {
            if (link is not null)
            {
                Marshal.FinalReleaseComObject(link);
            }
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

    [ComImport]
    [Guid("B92B56A9-8B55-4E14-9A89-0199BBB6F93B")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IDesktopWallpaper
    {
        void SetWallpaper(
            [MarshalAs(UnmanagedType.LPWStr)] string? monitorId,
            [MarshalAs(UnmanagedType.LPWStr)] string wallpaper);

        [return: MarshalAs(UnmanagedType.LPWStr)]
        string? GetWallpaper([MarshalAs(UnmanagedType.LPWStr)] string? monitorId);

        [return: MarshalAs(UnmanagedType.LPWStr)]
        string GetMonitorDevicePathAt(uint monitorIndex);

        uint GetMonitorDevicePathCount();
    }

    [ComImport]
    [Guid("000214F9-0000-0000-C000-000000000046")]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    private interface IShellLinkW
    {
        void GetPath(
            [Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder file,
            int fileCount,
            out Win32FindData findData,
            uint flags);

        void GetIDList(out nint itemIdList);
        void SetIDList(nint itemIdList);
        void GetDescription([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder name, int nameCount);
        void SetDescription([MarshalAs(UnmanagedType.LPWStr)] string name);
        void GetWorkingDirectory([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder directory, int directoryCount);
        void SetWorkingDirectory([MarshalAs(UnmanagedType.LPWStr)] string directory);
        void GetArguments([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder arguments, int argumentCount);
        void SetArguments([MarshalAs(UnmanagedType.LPWStr)] string arguments);
        void GetHotkey(out short hotkey);
        void SetHotkey(short hotkey);
        void GetShowCmd(out int showCommand);
        void SetShowCmd(int showCommand);
        void GetIconLocation([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder iconPath, int iconPathCount, out int iconIndex);
        void SetIconLocation([MarshalAs(UnmanagedType.LPWStr)] string iconPath, int iconIndex);
        void SetRelativePath([MarshalAs(UnmanagedType.LPWStr)] string path, uint reserved);
        void Resolve(nint window, uint flags);
        void SetPath([MarshalAs(UnmanagedType.LPWStr)] string path);
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct Win32FindData
    {
        public uint FileAttributes;
        public System.Runtime.InteropServices.ComTypes.FILETIME CreationTime;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastAccessTime;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWriteTime;
        public uint FileSizeHigh;
        public uint FileSizeLow;
        public uint Reserved0;
        public uint Reserved1;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string? FileName;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)]
        public string? AlternateFileName;
    }
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
