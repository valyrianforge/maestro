#include <catch2/catch_test_macros.hpp>

#include "maestro/runtime/ResultCollector.hpp"

using namespace maestro::core;
using maestro::runtime::ResultCollector;

namespace {
TaskChunk assistant(std::string text) {
    TaskChunk c;
    c.kind = TaskChunk::Kind::AssistantText;
    c.text = std::move(text);
    return c;
}
} // namespace

TEST_CASE("assistant text is concatenated when there is no result frame", "[collector]") {
    ResultCollector c;
    c.add(assistant("Hello "));
    c.add(assistant("world"));
    const auto r = c.finalize(true);
    REQUIRE(r.success);
    REQUIRE(r.output == "Hello world");
}

TEST_CASE("a result frame overrides accumulated assistant text", "[collector]") {
    ResultCollector c;
    c.add(assistant("streamed partial"));
    TaskChunk result;
    result.kind = TaskChunk::Kind::Result;
    result.text = "FINAL ANSWER";
    result.costUsd = 0.02;
    result.session = SessionId{"sess-1"};
    c.add(result);

    const auto r = c.finalize(true);
    REQUIRE(r.output == "FINAL ANSWER");
    REQUIRE(r.costUsd == 0.02);
    REQUIRE(r.session->value() == "sess-1");
}

TEST_CASE("stdout is the fallback output for non-structured tools", "[collector]") {
    ResultCollector c;
    TaskChunk line;
    line.kind = TaskChunk::Kind::Stdout;
    line.text = "plain output";
    c.add(line);
    REQUIRE(c.finalize(true).output == "plain output");
}

TEST_CASE("an error frame marks the result as failed even if the process exited 0",
          "[collector]") {
    ResultCollector c;
    TaskChunk err;
    err.kind = TaskChunk::Kind::Error;
    err.text = "rate limited";
    err.isError = true;
    c.add(err);
    REQUIRE_FALSE(c.finalize(true).success);
}

TEST_CASE("process failure fails the result regardless of content", "[collector]") {
    ResultCollector c;
    c.add(assistant("some text"));
    REQUIRE_FALSE(c.finalize(false).success);
}
