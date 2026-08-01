// =============================================================================
// Views Explorer Welcome POC
// File: tests/ExplorerWelcome.Broker.Tests/MetricsContractTests.cs
// Purpose: Verifies the lightweight live-metrics named-pipe contract.
//
// Copyright (c) 2026 Umberto Giacobbi
// Author: Umberto Giacobbi
// Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
// License: PolyForm Noncommercial License 1.0.0
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Open source: https://umbertogiacobbi.biz/opensource
// =============================================================================

using System.Text.Json;
using ExplorerWelcome.Broker;
using ExplorerWelcome.Contracts;
using Xunit;

namespace ExplorerWelcome.Broker.Tests;

public sealed class MetricsContractTests
{
    private static readonly JsonSerializerOptions Json = new(JsonSerializerDefaults.Web)
    {
        MaxDepth = 16
    };

    // Verifies that the additive metrics request remains valid under the current protocol version.
    [Fact]
    public void MetricsRequestIsAccepted()
    {
        var line = $$"""{"version":2,"type":"{{PipeProtocol.MetricsRequest}}","correlationId":"metrics-1"}""";

        var result = PipeRequestParser.Parse(line, Json);

        Assert.Null(result.Error);
        Assert.Equal(PipeProtocol.MetricsRequest, result.Request?.Type);
    }

    // Verifies that live metrics are serialized independently from the heavier welcome snapshot.
    [Fact]
    public void MetricsResponseUsesDedicatedPayload()
    {
        var metrics = new MetricsSnapshot
        {
            CpuPercent = 12.5,
            CpuSparkline = new[] { 10d, 12.5d },
            SampledUtc = DateTimeOffset.Parse("2026-07-28T12:00:00Z")
        };
        var response = new PipeResponse(
            PipeProtocol.CurrentVersion,
            "metrics.response",
            "metrics-2",
            Metrics: metrics);

        using var document = JsonDocument.Parse(JsonSerializer.Serialize(response, Json));
        var root = document.RootElement;

        Assert.Equal("metrics.response", root.GetProperty("type").GetString());
        Assert.Equal(12.5, root.GetProperty("metrics").GetProperty("cpuPercent").GetDouble());
        Assert.Equal(JsonValueKind.Null, root.GetProperty("snapshot").ValueKind);
    }
}
