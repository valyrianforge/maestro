#include <catch2/catch_test_macros.hpp>

#include "maestro/providers/NdjsonLineReader.hpp"

using maestro::providers::NdjsonLineReader;

TEST_CASE("splits multiple complete lines in one feed", "[ndjson]") {
    NdjsonLineReader r;
    const auto lines = r.feed("a\nb\nc\n");
    REQUIRE(lines == std::vector<std::string>{"a", "b", "c"});
}

TEST_CASE("buffers a partial line across feeds", "[ndjson]") {
    NdjsonLineReader r;
    REQUIRE(r.feed("{\"ty").empty());
    REQUIRE(r.feed("pe\":1}").empty());          // still no newline
    const auto lines = r.feed("\nnext\n");
    REQUIRE(lines == std::vector<std::string>{"{\"type\":1}", "next"});
}

TEST_CASE("retains a trailing partial line and flush() emits it", "[ndjson]") {
    NdjsonLineReader r;
    const auto lines = r.feed("done\ntrailing-no-newline");
    REQUIRE(lines == std::vector<std::string>{"done"});
    const auto tail = r.flush();
    REQUIRE(tail.has_value());
    REQUIRE(*tail == "trailing-no-newline");
}

TEST_CASE("strips carriage returns for CRLF streams", "[ndjson]") {
    NdjsonLineReader r;
    const auto lines = r.feed("a\r\nb\r\n");
    REQUIRE(lines == std::vector<std::string>{"a", "b"});
}

TEST_CASE("flush on empty buffer yields nothing", "[ndjson]") {
    NdjsonLineReader r;
    r.feed("x\n");
    REQUIRE_FALSE(r.flush().has_value());
}
