using ExplorerWelcome.Broker;
using Xunit;

namespace ExplorerWelcome.Broker.Tests;

public sealed class BoundedLineReaderTests
{
    [Fact]
    public async Task ReadsCrLfTerminatedLine()
    {
        using var reader = new StringReader("request\r\nignored");

        var result = await BoundedLineReader.ReadAsync(reader, 32);

        Assert.False(result.IsTooLong);
        Assert.Equal("request", result.Line);
    }

    [Fact]
    public async Task AcceptsLineAtExactLimit()
    {
        using var reader = new StringReader("1234\n");

        var result = await BoundedLineReader.ReadAsync(reader, 4);

        Assert.False(result.IsTooLong);
        Assert.Equal("1234", result.Line);
    }

    [Fact]
    public async Task RejectsLinePastLimitWithoutBufferingRemainder()
    {
        using var reader = new StringReader(new string('x', 10_000));

        var result = await BoundedLineReader.ReadAsync(reader, 64);

        Assert.True(result.IsTooLong);
        Assert.Null(result.Line);
    }

    [Fact]
    public async Task ReturnsNullForImmediateEndOfStream()
    {
        using var reader = new StringReader(string.Empty);

        var result = await BoundedLineReader.ReadAsync(reader, 64);

        Assert.False(result.IsTooLong);
        Assert.Null(result.Line);
    }
}
