/* =============================================================================
 * Views Explorer Welcome POC
 * File: src/ExplorerWelcome.NativeUi/broker_client.h
 * Purpose: Bounded native named-pipe client shared by the standalone and Explorer hosts.
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

#include <windows.h>

#include <array>
#include <atomic>
#include <string>
#include <string_view>
#include <thread>

#include "dashboard_snapshot.h"

namespace ExplorerWelcome::NativeUi::BrokerClient
{
inline constexpr wchar_t PipeName[] = LR"(\\.\pipe\views-explorer-welcome-poc)";
inline constexpr std::size_t MaxResponseBytes = 64 * 1024;

inline std::string EscapeJson(std::wstring_view value)
{
    const int utf8Size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (utf8Size <= 0)
    {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(utf8Size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        utf8.data(),
        utf8Size,
        nullptr,
        nullptr);

    std::string escaped;
    escaped.reserve(utf8.size());
    for (const unsigned char character : utf8)
    {
        switch (character)
        {
        case '\\': escaped += "\\\\"; break;
        case '"': escaped += "\\\""; break;
        case '\r': escaped += "\\r"; break;
        case '\n': escaped += "\\n"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (character >= 0x20)
            {
                escaped.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    return escaped;
}

inline std::string CorrelationId()
{
    GUID value{};
    if (FAILED(CoCreateGuid(&value)))
    {
        return "native-request";
    }

    wchar_t text[40]{};
    StringFromGUID2(value, text, static_cast<int>(std::size(text)));
    return EscapeJson(text);
}

inline bool SendRequest(std::string const& request, std::string& response)
{
    response.clear();
    if (request.empty() || request.size() > MaxResponseBytes || !WaitNamedPipeW(PipeName, 500))
    {
        return false;
    }

    HANDLE pipe = CreateFileW(
        PipeName,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    const std::string line = request + "\n";
    DWORD written{};
    const bool sent =
        WriteFile(pipe, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) != FALSE &&
        written == static_cast<DWORD>(line.size());
    if (!sent)
    {
        CloseHandle(pipe);
        return false;
    }

    std::array<char, 4096> buffer{};
    bool complete = false;
    while (response.size() < MaxResponseBytes)
    {
        DWORD read{};
        if (!ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) || read == 0)
        {
            break;
        }

        response.append(buffer.data(), read);
        const auto newline = response.find('\n');
        if (newline != std::string::npos)
        {
            response.resize(newline);
            complete = true;
            break;
        }
    }

    CloseHandle(pipe);
    return complete;
}

inline bool Ping()
{
    std::string response;
    const std::string request =
        "{\"version\":2,\"type\":\"host.ping\",\"correlationId\":\"" +
        CorrelationId() + "\"}";
    return SendRequest(request, response) &&
        response.find("\"type\":\"host.pong\"") != std::string::npos;
}

inline bool RequestSnapshot(NativeSnapshot& snapshot, std::wstring& summary)
{
    std::string response;
    const std::string request =
        "{\"version\":2,\"type\":\"snapshot.request\",\"correlationId\":\"" +
        CorrelationId() + "\"}";
    if (!SendRequest(request, response))
    {
        summary = L"Broker offline · dati locali mantenuti";
        return false;
    }
    return ParseSnapshotResponse(response, snapshot, summary);
}

inline NativeSnapshot LoadCachedSnapshot()
{
    NativeSnapshot snapshot;
    wchar_t localAppData[32768]{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        localAppData,
        static_cast<DWORD>(std::size(localAppData)));
    if (length == 0 || length >= static_cast<DWORD>(std::size(localAppData)))
    {
        return snapshot;
    }

    const std::wstring path =
        std::wstring(localAppData, length) + L"\\ViewsExplorerWelcome\\snapshot-cache.json";
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return snapshot;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > static_cast<LONGLONG>(MaxResponseBytes))
    {
        CloseHandle(file);
        return snapshot;
    }

    std::string cached(static_cast<std::size_t>(size.QuadPart), '\0');
    DWORD read{};
    const bool loaded =
        ReadFile(file, cached.data(), static_cast<DWORD>(cached.size()), &read, nullptr) != FALSE &&
        read == static_cast<DWORD>(cached.size());
    CloseHandle(file);
    if (!loaded)
    {
        return snapshot;
    }

    std::wstring ignored;
    const std::string envelope =
        "{\"type\":\"snapshot.response\",\"snapshot\":" + cached + "}";
    ParseSnapshotResponse(envelope, snapshot, ignored);
    snapshot.IsStale = snapshot.IsLoaded;
    return snapshot;
}

inline void LaunchActionAsync(std::wstring action, std::wstring target)
{
    static std::atomic_uint32_t inFlightActions{ 0 };
    if (inFlightActions.fetch_add(1) >= 4)
    {
        inFlightActions.fetch_sub(1);
        return;
    }

    try
    {
        std::thread([action = std::move(action), target = std::move(target)]
        {
            try
            {
                const std::string request =
                    "{\"version\":2,\"type\":\"action.request\",\"correlationId\":\"" +
                    CorrelationId() + "\",\"action\":\"" + EscapeJson(action) +
                    "\",\"target\":\"" + EscapeJson(target) + "\"}";
                std::string ignored;
                SendRequest(request, ignored);
            }
            catch (...)
            {
                // Detached action failures are intentionally isolated from Explorer.
            }
            inFlightActions.fetch_sub(1);
        }).detach();
    }
    catch (...)
    {
        inFlightActions.fetch_sub(1);
    }
}
}
