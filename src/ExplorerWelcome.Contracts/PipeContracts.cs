// =============================================================================
// Views Explorer Welcome POC
// File: src/ExplorerWelcome.Contracts/PipeContracts.cs
// Purpose: Versioned, bounded named-pipe contract for the Explorer Home V2 dashboard.
//
// Copyright (c) 2026 Umberto Giacobbi
// Author: Umberto Giacobbi
// Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
// License: PolyForm Noncommercial License 1.0.0
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Open source: https://umbertogiacobbi.biz/opensource
// =============================================================================

namespace ExplorerWelcome.Contracts;

public static class PipeProtocol
{
    public const int CurrentVersion = 2;
    public const string PipeName = "views-explorer-welcome-poc";
    public const int MaxLineLength = 64 * 1024;
    public const int MaxCollectionCount = 64;
    public const string SnapshotRequest = "snapshot.request";
    public const string MetricsRequest = "metrics.request";
    public const string HostPingRequest = "host.ping";
    public const string ActionRequest = "action.request";
    public const string PreferencesRequest = "preferences.update";
}

public sealed record PipeRequest(
    int Version,
    string Type,
    string CorrelationId,
    string? Action = null,
    string? Target = null,
    IReadOnlyDictionary<string, string>? Arguments = null);

public sealed record PipeResponse(
    int Version,
    string Type,
    string CorrelationId,
    WelcomePageSnapshot? Snapshot = null,
    ActionResult? ActionResult = null,
    string? Error = null,
    MetricsSnapshot? Metrics = null);

public sealed record ActionResult(
    bool Accepted,
    string Action,
    string? Message = null);

public sealed record WelcomePageSnapshot
{
    public int Version { get; init; } = PipeProtocol.CurrentVersion;
    public DateTimeOffset GeneratedUtc { get; init; } = DateTimeOffset.UtcNow;
    public string Title { get; init; } = "Views Home";
    public string Subtitle { get; init; } = "A calm, useful starting point for your files.";
    public MachineSnapshot Machine { get; init; } = new();
    public MetricsSnapshot Metrics { get; init; } = new();
    public IReadOnlyList<StorageSnapshot> Storage { get; init; } = Array.Empty<StorageSnapshot>();
    public IReadOnlyList<NetworkLocationSnapshot> NetworkLocations { get; init; } = Array.Empty<NetworkLocationSnapshot>();
    public IReadOnlyList<RecentItemSnapshot> RecentItems { get; init; } = Array.Empty<RecentItemSnapshot>();
    public IReadOnlyList<HighlightedItemSnapshot> HighlightedItems { get; init; } = Array.Empty<HighlightedItemSnapshot>();
    public IReadOnlyList<TerminalProfileSnapshot> TerminalProfiles { get; init; } = Array.Empty<TerminalProfileSnapshot>();
    public IReadOnlyList<ToolSnapshot> Tools { get; init; } = Array.Empty<ToolSnapshot>();
    public IReadOnlyList<QuickSettingSnapshot> QuickSettings { get; init; } = Array.Empty<QuickSettingSnapshot>();
    public PreferencesSnapshot Preferences { get; init; } = new();
    public FreshnessSnapshot Freshness { get; init; } = new();
}

public sealed record MachineSnapshot
{
    public string Name { get; init; } = "Unknown";
    public string Model { get; init; } = "Unknown";
    public string OsDisplayVersion { get; init; } = "Unknown";
    public string? PrimaryNetworkIdentity { get; init; }
    public string CpuModel { get; init; } = "Unknown";
    public string GpuModel { get; init; } = "Unavailable";
    public long MemoryUsedBytes { get; init; }
    public long MemoryTotalBytes { get; init; }
    public string? WallpaperPath { get; init; }
}

public sealed record MetricsSnapshot
{
    public double? CpuPercent { get; init; }
    public double? GpuPercent { get; init; }
    public double? MemoryPercent { get; init; }
    public double? StoragePercent { get; init; }
    public long? NetworkSendBytesPerSecond { get; init; }
    public long? NetworkReceiveBytesPerSecond { get; init; }
    public IReadOnlyList<double> CpuSparkline { get; init; } = Array.Empty<double>();
    public IReadOnlyList<double> NetworkSparkline { get; init; } = Array.Empty<double>();
    public DateTimeOffset SampledUtc { get; init; } = DateTimeOffset.UtcNow;
}

public sealed record StorageSnapshot
{
    public string Id { get; init; } = string.Empty;
    public string Path { get; init; } = string.Empty;
    public string? Label { get; init; }
    public string Kind { get; init; } = "Unknown";
    public long? FreeBytes { get; init; }
    public long? TotalBytes { get; init; }
    public string? FileSystem { get; init; }
    public string Health { get; init; } = "Unknown";
    public bool IsReady { get; init; }
    public bool IsRemovable { get; init; }
    public bool IsNetwork { get; init; }
    public string? Error { get; init; }
}

public sealed record NetworkLocationSnapshot
{
    public string Id { get; init; } = string.Empty;
    public string Path { get; init; } = string.Empty;
    public string DisplayName { get; init; } = string.Empty;
    public string State { get; init; } = "Unknown";
    public long? FreeBytes { get; init; }
    public long? TotalBytes { get; init; }
    public string? Error { get; init; }
}

public sealed record RecentItemSnapshot
{
    public string Id { get; init; } = string.Empty;
    public string DisplayName { get; init; } = string.Empty;
    public string? Parent { get; init; }
    public string? TargetPath { get; init; }
    public string TargetKind { get; init; } = "Unknown";
    public DateTimeOffset? TimestampUtc { get; init; }
    public bool IsAvailable { get; init; }
}

public sealed record HighlightedItemSnapshot
{
    public string Source { get; init; } = "Windows";
    public string Id { get; init; } = string.Empty;
    public string DisplayName { get; init; } = string.Empty;
    public string? TargetPath { get; init; }
    public string TargetKind { get; init; } = "Unknown";
    public bool CanPin { get; init; }
    public string? Error { get; init; }
}

public sealed record TerminalProfileSnapshot
{
    public string Id { get; init; } = string.Empty;
    public string DisplayName { get; init; } = string.Empty;
    public string Executable { get; init; } = string.Empty;
    public bool IsAvailable { get; init; }
    public string? Version { get; init; }
    public IReadOnlyList<string> SupportedTargetKinds { get; init; } = Array.Empty<string>();
}

public sealed record ToolSnapshot
{
    public string Id { get; init; } = string.Empty;
    public string DisplayName { get; init; } = string.Empty;
    public string Executable { get; init; } = string.Empty;
    public bool IsEnabled { get; init; }
    public string? Version { get; init; }
    public IReadOnlyList<string> SupportedTargetKinds { get; init; } = Array.Empty<string>();
}

public sealed record QuickSettingSnapshot
{
    public string Id { get; init; } = string.Empty;
    public string DisplayName { get; init; } = string.Empty;
    public string Uri { get; init; } = string.Empty;
    public bool IsVisible { get; init; } = true;
}

public sealed record PreferencesSnapshot
{
    public bool HeroCollapsed { get; init; }
    public IReadOnlyList<string> VisibleModules { get; init; } = new[] { "Storage", "Network", "Recent", "Highlighted", "Terminal" };
    public IReadOnlyList<string> QuickSettingOrder { get; init; } = Array.Empty<string>();
    public string DefaultWorkingDirectory { get; init; } = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    public string? PreferredTerminalProfile { get; init; }
    public string FolderNavigation { get; init; } = "SameExplorerTab";
    public int CapacityWarningFreePercent { get; init; } = 10;
    public int CapacityCriticalFreePercent { get; init; } = 5;
    public bool PrivacyShowNetworkIdentity { get; init; } = true;
    public bool PrivacyShowDeviceDetails { get; init; } = true;
    public string MetricRefreshMode { get; init; } = "Live";
}

public sealed record FreshnessSnapshot
{
    public DateTimeOffset GeneratedUtc { get; init; } = DateTimeOffset.UtcNow;
    public bool BrokerAvailable { get; init; } = true;
    public bool IsStale { get; init; }
    public IReadOnlyDictionary<string, string> SectionErrors { get; init; } = new Dictionary<string, string>();
}

// Kept for consumers of the first feasibility contract.
[Obsolete("Use StorageSnapshot in WelcomePageSnapshot.Storage.")]
public sealed record DriveSnapshot(string Name, string? Label, long FreeBytes, long TotalBytes);
