/* =============================================================================
 * Views Explorer Welcome POC
 * File: src/ExplorerWelcome.NativeUi/dashboard_snapshot.h
 * Purpose: Compact native projection of the versioned broker snapshot.
 *
 * Copyright (c) 2026 Umberto Giacobbi
 * Author: Umberto Giacobbi
 * Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
 * License: PolyForm Noncommercial License 1.0.0
 * SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
 * Open source: https://umbertogiacobbi.biz/opensource
 * =============================================================================
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <winrt/base.h>
#include <winrt/Windows.Data.Json.h>

namespace ExplorerWelcome::NativeUi
{
struct NativeStorageItem
{
    std::wstring Id;
    std::wstring Path;
    std::wstring Label;
    std::wstring Kind;
    std::optional<std::uint64_t> FreeBytes;
    std::optional<std::uint64_t> TotalBytes;
    std::wstring FileSystem;
    std::wstring Health{ L"Unknown" };
    bool IsReady{};
    bool IsNetwork{};
    std::wstring Error;
};

struct NativeListItem
{
    std::wstring Id;
    std::wstring DisplayName;
    std::wstring Detail;
    std::wstring Target;
    std::wstring TargetKind;
    bool IsAvailable{};
};

struct NativeQuickSetting
{
    std::wstring Id;
    std::wstring DisplayName;
    std::wstring Uri;
};

struct NativeSnapshot
{
    bool IsLoaded{};
    bool IsStale{};
    std::wstring GeneratedUtc;
    std::wstring MachineName{ L"WORKSTATION" };
    std::wstring MachineModel{ L"Views Explorer Home V2" };
    std::wstring OsDisplayVersion;
    std::wstring PrimaryNetworkIdentity;
    std::wstring CpuModel{ L"CPU non disponibile" };
    std::wstring GpuModel{ L"GPU non disponibile" };
    std::wstring WallpaperPath;
    std::uint64_t MemoryUsedBytes{};
    std::uint64_t MemoryTotalBytes{};
    std::optional<double> CpuPercent;
    std::optional<double> GpuPercent;
    std::optional<double> MemoryPercent;
    std::optional<double> StoragePercent;
    std::optional<std::uint64_t> NetworkSendBytesPerSecond;
    std::optional<std::uint64_t> NetworkReceiveBytesPerSecond;
    std::vector<NativeStorageItem> Storage;
    std::vector<NativeListItem> NetworkLocations;
    std::vector<NativeListItem> RecentItems;
    std::vector<NativeListItem> HighlightedItems;
    std::vector<NativeListItem> TerminalProfiles;
    std::vector<NativeListItem> Tools;
    std::vector<NativeQuickSetting> QuickSettings;
    std::wstring HighlightedUnavailableReason;
};

namespace Json
{
using winrt::Windows::Data::Json::IJsonValue;
using winrt::Windows::Data::Json::JsonArray;
using winrt::Windows::Data::Json::JsonObject;
using winrt::Windows::Data::Json::JsonValueType;

inline std::wstring String(
    JsonObject const& object,
    wchar_t const* name,
    std::wstring const& fallback = {})
{
    if (object && object.HasKey(name))
    {
        const auto value = object.Lookup(name);
        if (value.ValueType() == JsonValueType::String)
        {
            return std::wstring(value.GetString().c_str());
        }
    }
    return fallback;
}

inline std::optional<double> Number(JsonObject const& object, wchar_t const* name)
{
    if (object && object.HasKey(name))
    {
        const auto value = object.Lookup(name);
        if (value.ValueType() == JsonValueType::Number)
        {
            return value.GetNumber();
        }
    }
    return std::nullopt;
}

inline std::optional<std::uint64_t> Unsigned(JsonObject const& object, wchar_t const* name)
{
    const auto value = Number(object, name);
    if (!value || *value < 0)
    {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(*value);
}

inline bool Boolean(JsonObject const& object, wchar_t const* name, bool fallback = false)
{
    if (object && object.HasKey(name))
    {
        const auto value = object.Lookup(name);
        if (value.ValueType() == JsonValueType::Boolean)
        {
            return value.GetBoolean();
        }
    }
    return fallback;
}

inline JsonObject Object(JsonObject const& object, wchar_t const* name)
{
    if (object && object.HasKey(name))
    {
        const auto value = object.Lookup(name);
        if (value.ValueType() == JsonValueType::Object)
        {
            return value.GetObject();
        }
    }
    return nullptr;
}

inline JsonArray Array(JsonObject const& object, wchar_t const* name)
{
    if (object && object.HasKey(name))
    {
        const auto value = object.Lookup(name);
        if (value.ValueType() == JsonValueType::Array)
        {
            return value.GetArray();
        }
    }
    return nullptr;
}
}

inline bool ParseSnapshotResponse(
    std::string const& response,
    NativeSnapshot& result,
    std::wstring& summary)
{
    using namespace winrt::Windows::Data::Json;

    try
    {
        if (response.size() > 64 * 1024)
        {
            summary = L"Risposta broker troppo grande";
            return false;
        }

        const auto envelope = JsonObject::Parse(winrt::to_hstring(response));
        const auto responseType = Json::String(envelope, L"type");
        if (responseType == L"snapshot.stale")
        {
            summary = L"Snapshot stale · dati locali mantenuti";
            return false;
        }
        if (responseType != L"snapshot.response")
        {
            summary = L"Risposta snapshot non valida";
            return false;
        }

        const auto snapshot = Json::Object(envelope, L"snapshot");
        if (!snapshot)
        {
            summary = L"Snapshot broker mancante";
            return false;
        }

        NativeSnapshot parsed;
        parsed.IsLoaded = true;
        parsed.GeneratedUtc = Json::String(snapshot, L"generatedUtc");

        const auto machine = Json::Object(snapshot, L"machine");
        parsed.MachineName = Json::String(machine, L"name", L"Computer");
        parsed.MachineModel = Json::String(machine, L"model", L"Modello non disponibile");
        parsed.OsDisplayVersion = Json::String(machine, L"osDisplayVersion");
        parsed.PrimaryNetworkIdentity = Json::String(machine, L"primaryNetworkIdentity");
        parsed.CpuModel = Json::String(machine, L"cpuModel", L"CPU non disponibile");
        parsed.GpuModel = Json::String(machine, L"gpuModel", L"GPU non disponibile");
        parsed.WallpaperPath = Json::String(machine, L"wallpaperPath");
        parsed.MemoryUsedBytes = Json::Unsigned(machine, L"memoryUsedBytes").value_or(0);
        parsed.MemoryTotalBytes = Json::Unsigned(machine, L"memoryTotalBytes").value_or(0);

        const auto metrics = Json::Object(snapshot, L"metrics");
        parsed.CpuPercent = Json::Number(metrics, L"cpuPercent");
        parsed.GpuPercent = Json::Number(metrics, L"gpuPercent");
        parsed.MemoryPercent = Json::Number(metrics, L"memoryPercent");
        parsed.StoragePercent = Json::Number(metrics, L"storagePercent");
        parsed.NetworkSendBytesPerSecond = Json::Unsigned(metrics, L"networkSendBytesPerSecond");
        parsed.NetworkReceiveBytesPerSecond = Json::Unsigned(metrics, L"networkReceiveBytesPerSecond");

        const auto storage = Json::Array(snapshot, L"storage");
        if (storage)
        {
            const auto count = (std::min)(storage.Size(), 64u);
            parsed.Storage.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto item = storage.GetObjectAt(index);
                parsed.Storage.push_back(NativeStorageItem{
                    Json::String(item, L"id"),
                    Json::String(item, L"path"),
                    Json::String(item, L"label"),
                    Json::String(item, L"kind"),
                    Json::Unsigned(item, L"freeBytes"),
                    Json::Unsigned(item, L"totalBytes"),
                    Json::String(item, L"fileSystem"),
                    Json::String(item, L"health", L"Unknown"),
                    Json::Boolean(item, L"isReady"),
                    Json::Boolean(item, L"isNetwork"),
                    Json::String(item, L"error")
                });
            }
        }

        const auto network = Json::Array(snapshot, L"networkLocations");
        if (network)
        {
            const auto count = (std::min)(network.Size(), 64u);
            parsed.NetworkLocations.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto item = network.GetObjectAt(index);
                const auto state = Json::String(item, L"state", L"Unknown");
                parsed.NetworkLocations.push_back(NativeListItem{
                    Json::String(item, L"id"),
                    Json::String(item, L"displayName", Json::String(item, L"path")),
                    state,
                    Json::String(item, L"path"),
                    L"Network",
                    state == L"Connected"
                });
            }
        }

        const auto recent = Json::Array(snapshot, L"recentItems");
        if (recent)
        {
            const auto count = (std::min)(recent.Size(), 5u);
            parsed.RecentItems.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto item = recent.GetObjectAt(index);
                parsed.RecentItems.push_back(NativeListItem{
                    Json::String(item, L"id"),
                    Json::String(item, L"displayName"),
                    Json::String(item, L"parent"),
                    Json::String(item, L"targetPath"),
                    Json::String(item, L"targetKind"),
                    Json::Boolean(item, L"isAvailable")
                });
            }
        }

        const auto highlighted = Json::Array(snapshot, L"highlightedItems");
        if (highlighted)
        {
            const auto count = (std::min)(highlighted.Size(), 8u);
            parsed.HighlightedItems.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto item = highlighted.GetObjectAt(index);
                parsed.HighlightedItems.push_back(NativeListItem{
                    Json::String(item, L"id"),
                    Json::String(item, L"displayName"),
                    Json::String(item, L"source", L"Windows"),
                    Json::String(item, L"targetPath"),
                    Json::String(item, L"targetKind"),
                    !Json::String(item, L"targetPath").empty()
                });
            }
        }

        const auto terminals = Json::Array(snapshot, L"terminalProfiles");
        if (terminals)
        {
            const auto count = (std::min)(terminals.Size(), 8u);
            parsed.TerminalProfiles.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto item = terminals.GetObjectAt(index);
                parsed.TerminalProfiles.push_back(NativeListItem{
                    Json::String(item, L"id"),
                    Json::String(item, L"displayName"),
                    Json::Boolean(item, L"isAvailable") ? L"Disponibile" : L"Non disponibile",
                    Json::String(item, L"id"),
                    L"Tool",
                    Json::Boolean(item, L"isAvailable")
                });
            }
        }

        const auto tools = Json::Array(snapshot, L"tools");
        if (tools)
        {
            const auto count = (std::min)(tools.Size(), 16u);
            parsed.Tools.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto item = tools.GetObjectAt(index);
                parsed.Tools.push_back(NativeListItem{
                    Json::String(item, L"id"),
                    Json::String(item, L"displayName"),
                    Json::Boolean(item, L"isEnabled") ? L"Disponibile" : L"Non disponibile",
                    Json::String(item, L"id"),
                    L"Tool",
                    Json::Boolean(item, L"isEnabled")
                });
            }
        }

        const auto quickSettings = Json::Array(snapshot, L"quickSettings");
        if (quickSettings)
        {
            const auto count = (std::min)(quickSettings.Size(), 16u);
            parsed.QuickSettings.reserve(count);
            for (std::uint32_t index = 0; index < count; ++index)
            {
                const auto item = quickSettings.GetObjectAt(index);
                if (Json::Boolean(item, L"isVisible", true))
                {
                    parsed.QuickSettings.push_back(NativeQuickSetting{
                        Json::String(item, L"id"),
                        Json::String(item, L"displayName"),
                        Json::String(item, L"uri")
                    });
                }
            }
        }

        const auto freshness = Json::Object(snapshot, L"freshness");
        parsed.IsStale = Json::Boolean(freshness, L"isStale");
        const auto sectionErrors = Json::Object(freshness, L"sectionErrors");
        parsed.HighlightedUnavailableReason = Json::String(sectionErrors, L"highlightedItems");

        result = std::move(parsed);
        summary = result.IsStale
            ? L"Informazioni potenzialmente non aggiornate"
            : L"Snapshot aggiornato dal broker";
        return true;
    }
    catch (winrt::hresult_error const&)
    {
        summary = L"Risposta broker malformata · dati locali mantenuti";
        return false;
    }
}
}
