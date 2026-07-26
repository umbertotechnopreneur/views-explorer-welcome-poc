using System.Text.Json;
using ExplorerWelcome.Broker;
using ExplorerWelcome.Contracts;
using Xunit;

namespace ExplorerWelcome.Broker.Tests;

public sealed class PipeRequestParserTests
{
    private static readonly JsonSerializerOptions Json = new(JsonSerializerDefaults.Web)
    {
        MaxDepth = 16
    };

    [Fact]
    public void ValidPingRequestIsAccepted()
    {
        var line = """{"version":2,"type":"host.ping","correlationId":"test-1"}""";

        var result = PipeRequestParser.Parse(line, Json);

        Assert.Null(result.Error);
        Assert.NotNull(result.Request);
        Assert.Equal("test-1", result.CorrelationId);
    }

    [Theory]
    [InlineData("")]
    [InlineData("  ")]
    [InlineData("{not-json}")]
    [InlineData("null")]
    public void EmptyOrMalformedRequestIsRejected(string line)
    {
        var result = PipeRequestParser.Parse(line, Json);

        Assert.NotNull(result.Error);
        Assert.Null(result.Request);
    }

    [Fact]
    public void UnsupportedVersionIsRejected()
    {
        var line = """{"version":999,"type":"host.ping","correlationId":"test-2"}""";

        var result = PipeRequestParser.Parse(line, Json);

        Assert.Equal("Unsupported protocol version.", result.Error);
    }

    [Fact]
    public void OversizedRequestIsRejectedBeforeDeserialization()
    {
        var line = new string('x', PipeProtocol.MaxLineLength + 1);

        var result = PipeRequestParser.Parse(line, Json);

        Assert.Equal("Request is empty or too large.", result.Error);
    }

    [Fact]
    public void OversizedCorrelationIdIsNotReflected()
    {
        var correlation = new string('a', 129);
        var line = $$"""{"version":2,"type":"host.ping","correlationId":"{{correlation}}"}""";

        var result = PipeRequestParser.Parse(line, Json);

        Assert.Null(result.Error);
        Assert.NotEqual(correlation, result.CorrelationId);
        Assert.Equal(32, result.CorrelationId.Length);
    }

    [Fact]
    public void ControlCharactersInArgumentsAreRejected()
    {
        var line = """{"version":2,"type":"action.request","correlationId":"test-3","action":"folder.open","target":"C:\\","arguments":{"value":"line\u0000break"}}""";

        var result = PipeRequestParser.Parse(line, Json);

        Assert.Equal("Request arguments are invalid.", result.Error);
    }

    [Fact]
    public void ExcessiveJsonDepthIsRejected()
    {
        var nested = new string('[', 20) + "0" + new string(']', 20);
        var line = $$"""{"version":2,"type":"host.ping","correlationId":"test-4","extra":{{nested}}}""";

        var result = PipeRequestParser.Parse(line, Json);

        Assert.Equal("Request JSON is invalid.", result.Error);
    }

    [Theory]
    [InlineData("target", """{"version":2,"type":"action.request","correlationId":"test-5","target":"line\u000Abreak"}""")]
    [InlineData("arguments", """{"version":2,"type":"action.request","correlationId":"test-6","arguments":{"value":"line\u0009break"}}""")]
    public void ControlCharactersAreRejectedAcrossUserSuppliedValues(string _, string line)
    {
        var result = PipeRequestParser.Parse(line, Json);

        Assert.NotNull(result.Error);
    }
}
