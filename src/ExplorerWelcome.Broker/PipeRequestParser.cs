// =============================================================================
// Views Explorer Welcome POC
// File: src/ExplorerWelcome.Broker/PipeRequestParser.cs
// Purpose: Pure, bounded request parsing kept separate from named-pipe I/O.
//
// Copyright (c) 2026 Umberto Giacobbi
// Author: Umberto Giacobbi
// Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
// License: PolyForm Noncommercial License 1.0.0
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Open source: https://umbertogiacobbi.biz/opensource
// =============================================================================

using System.Text.Json;
using ExplorerWelcome.Contracts;

namespace ExplorerWelcome.Broker;

internal sealed record PipeParseResult(
    PipeRequest? Request,
    string CorrelationId,
    string? Error);

internal static class PipeRequestParser
{
    private const int MaxCorrelationIdLength = 128;
    private const int MaxTypeLength = 64;
    private const int MaxActionLength = 64;
    private const int MaxArgumentKeyLength = 64;
    private const int MaxArgumentValueLength = 4_096;

    public static PipeParseResult Parse(string? line, JsonSerializerOptions json)
    {
        if (string.IsNullOrWhiteSpace(line) || line.Length > PipeProtocol.MaxLineLength)
        {
            return Error("broker", "Request is empty or too large.");
        }

        PipeRequest? request;
        try
        {
            request = JsonSerializer.Deserialize<PipeRequest>(line, json);
        }
        catch (JsonException)
        {
            return Error("broker", "Request JSON is invalid.");
        }

        if (request is null)
        {
            return Error("broker", "Request JSON is invalid.");
        }

        var correlationId = IsBoundedText(request.CorrelationId, MaxCorrelationIdLength)
            ? request.CorrelationId
            : Guid.NewGuid().ToString("N");

        if (request.Version != PipeProtocol.CurrentVersion)
        {
            return Error(correlationId, "Unsupported protocol version.");
        }

        if (!IsBoundedText(request.Type, MaxTypeLength))
        {
            return Error(correlationId, "Request type is invalid.");
        }

        if (request.Action is not null && !IsBoundedText(request.Action, MaxActionLength))
        {
            return Error(correlationId, "Action name is invalid.");
        }

        if (request.Target is { Length: > MaxArgumentValueLength } ||
            request.Target?.Any(char.IsControl) == true)
        {
            return Error(correlationId, "Request target is invalid.");
        }

        if (request.Arguments is { Count: > 16 } ||
            request.Arguments?.Any(pair =>
                !IsBoundedText(pair.Key, MaxArgumentKeyLength) ||
                pair.Value is null ||
                pair.Value.Length > MaxArgumentValueLength ||
                pair.Value.Any(char.IsControl)) == true)
        {
            return Error(correlationId, "Request arguments are invalid.");
        }

        return new PipeParseResult(request, correlationId, null);
    }

    private static bool IsBoundedText(string? value, int maximumLength)
        => !string.IsNullOrWhiteSpace(value) &&
           value.Length <= maximumLength &&
           !value.Any(char.IsControl);

    private static PipeParseResult Error(string correlationId, string message)
        => new(null, correlationId, message);
}
