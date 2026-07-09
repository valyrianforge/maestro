#include <catch2/catch_test_macros.hpp>

#include "maestro/providers/ClaudeProvider.hpp"

using namespace maestro::core;
using maestro::providers::ClaudeProvider;

TEST_CASE("buildSpec emits headless stream-json with required --verbose", "[claude]") {
    ClaudeProvider provider;
    TaskRequest req;
    req.prompt = "hello world";

    const ProcessSpec spec = provider.buildSpec(req);
    REQUIRE(spec.program == "claude");
    REQUIRE(spec.args == std::vector<std::string>{"-p", "hello world", "--output-format",
                                                  "stream-json", "--verbose"});
}

TEST_CASE("buildSpec adds --resume when a session is provided", "[claude]") {
    ClaudeProvider provider;
    TaskRequest req;
    req.prompt = "continue";
    req.resume = SessionId{"d9567355-2047-42d8-81f0-7bd0c4487d22"};

    const ProcessSpec spec = provider.buildSpec(req);
    REQUIRE(spec.args.size() == 7);
    REQUIRE(spec.args[5] == "--resume");
    REQUIRE(spec.args[6] == "d9567355-2047-42d8-81f0-7bd0c4487d22");
}

TEST_CASE("parseFrame extracts assistant answer text", "[claude]") {
    ClaudeProvider provider;
    const auto chunk = provider.parseFrame(
        R"({"type":"assistant","message":{"role":"assistant","content":[{"type":"text","text":"Hi there, friend!"}]},"session_id":"abc-123"})");

    REQUIRE(chunk.has_value());
    REQUIRE(chunk->kind == TaskChunk::Kind::AssistantText);
    REQUIRE(chunk->text == "Hi there, friend!");
    REQUIRE(chunk->session.has_value());
    REQUIRE(chunk->session->value() == "abc-123");
}

TEST_CASE("parseFrame reads the terminal result with cost", "[claude]") {
    ClaudeProvider provider;
    const auto chunk = provider.parseFrame(
        R"({"type":"result","subtype":"success","is_error":false,"result":"Hi there, friend!","total_cost_usd":0.05,"session_id":"abc-123"})");

    REQUIRE(chunk.has_value());
    REQUIRE(chunk->kind == TaskChunk::Kind::Result);
    REQUIRE(chunk->text == "Hi there, friend!");
    REQUIRE_FALSE(chunk->isError);
    REQUIRE(chunk->costUsd.has_value());
    REQUIRE(*chunk->costUsd == 0.05);
}

TEST_CASE("parseFrame flags an error result", "[claude]") {
    ClaudeProvider provider;
    const auto chunk = provider.parseFrame(
        R"({"type":"result","subtype":"error","is_error":true,"result":"boom"})");

    REQUIRE(chunk.has_value());
    REQUIRE(chunk->kind == TaskChunk::Kind::Error);
    REQUIRE(chunk->isError);
}

TEST_CASE("parseFrame reports the session from an init frame", "[claude]") {
    ClaudeProvider provider;
    const auto chunk = provider.parseFrame(
        R"({"type":"system","subtype":"init","session_id":"sess-9","model":"claude-opus-4-8"})");

    REQUIRE(chunk.has_value());
    REQUIRE(chunk->kind == TaskChunk::Kind::SessionStarted);
    REQUIRE(chunk->session->value() == "sess-9");
}

TEST_CASE("parseFrame classifies hook/rate-limit noise as Other", "[claude]") {
    ClaudeProvider provider;
    const auto hook = provider.parseFrame(
        R"({"type":"system","subtype":"hook_started","session_id":"s"})");
    REQUIRE(hook.has_value());
    REQUIRE(hook->kind == TaskChunk::Kind::Other);
}

TEST_CASE("parseFrame tolerates junk without throwing", "[claude]") {
    ClaudeProvider provider;
    REQUIRE_FALSE(provider.parseFrame("").has_value());
    REQUIRE_FALSE(provider.parseFrame("not json at all").has_value());
    REQUIRE_FALSE(provider.parseFrame("[1,2,3]").has_value()); // valid json, not an object
}
