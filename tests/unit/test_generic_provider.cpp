#include <catch2/catch_test_macros.hpp>

#include "maestro/providers/GenericCliProvider.hpp"

using namespace maestro::core;
using maestro::providers::GenericCliProvider;

namespace {
GenericCliProvider makeEchoProvider() {
    return GenericCliProvider(GenericCliProvider::Config{
        ProviderId{"echo-tool"},
        "mytool",
        {"--query", "{{prompt}}", "--format", "text"},
        Capabilities{false, false, false}});
}
} // namespace

TEST_CASE("generic provider substitutes {{prompt}} in the arg template", "[generic]") {
    const auto provider = makeEchoProvider();
    TaskRequest req;
    req.prompt = "find bugs";

    const ProcessSpec spec = provider.buildSpec(req);
    REQUIRE(spec.program == "mytool");
    REQUIRE(spec.args == std::vector<std::string>{"--query", "find bugs", "--format", "text"});
}

TEST_CASE("generic provider treats each output line as stdout text", "[generic]") {
    const auto provider = makeEchoProvider();
    const auto chunk = provider.parseFrame("just some output");

    REQUIRE(chunk.has_value());
    REQUIRE(chunk->kind == TaskChunk::Kind::Stdout);
    REQUIRE(chunk->text == "just some output");
}

TEST_CASE("generic provider exposes its configured id and capabilities", "[generic]") {
    const auto provider = makeEchoProvider();
    REQUIRE(provider.id().name == "echo-tool");
    REQUIRE_FALSE(provider.capabilities().streaming);
}
