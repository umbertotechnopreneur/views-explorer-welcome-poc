// =============================================================================
// Views Explorer Welcome POC
// File: src/ExplorerWelcome.Broker/BoundedLineReader.cs
// Purpose: Bounded text framing for the current-user named-pipe protocol.
//
// Copyright (c) 2026 Umberto Giacobbi
// Author: Umberto Giacobbi
// Repository: https://github.com/umbertotechnopreneur/views-explorer-welcome-poc
// License: PolyForm Noncommercial License 1.0.0
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Open source: https://umbertogiacobbi.biz/opensource
// =============================================================================

using System.Text;

namespace ExplorerWelcome.Broker;

internal sealed record BoundedLineResult(string? Line, bool IsTooLong);

internal static class BoundedLineReader
{
    private const int BufferLength = 1_024;

    public static async ValueTask<BoundedLineResult> ReadAsync(
        TextReader reader,
        int maximumLength,
        CancellationToken cancellationToken = default)
    {
        ArgumentNullException.ThrowIfNull(reader);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(maximumLength);

        var line = new StringBuilder(Math.Min(maximumLength, 4_096));
        var buffer = new char[BufferLength];

        while (true)
        {
            var remainingWithSentinel = maximumLength - line.Length + 1;
            var requested = Math.Min(buffer.Length, remainingWithSentinel);
            var read = await reader.ReadAsync(
                buffer.AsMemory(0, requested),
                cancellationToken);

            if (read == 0)
            {
                return new BoundedLineResult(
                    line.Length == 0 ? null : line.ToString(),
                    false);
            }

            for (var index = 0; index < read; index++)
            {
                var character = buffer[index];
                if (character == '\n')
                {
                    if (line.Length > 0 && line[^1] == '\r')
                    {
                        line.Length--;
                    }

                    return new BoundedLineResult(line.ToString(), false);
                }

                if (line.Length >= maximumLength)
                {
                    return new BoundedLineResult(null, true);
                }

                line.Append(character);
            }
        }
    }
}
