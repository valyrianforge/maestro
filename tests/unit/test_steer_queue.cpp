#include <catch2/catch_test_macros.hpp>

#include "maestro/acp/SteerQueue.hpp"

using maestro::acp::SteerQueue;

TEST_CASE("a fresh steer queue is empty", "[steer]") {
    SteerQueue q;
    REQUIRE(q.empty());
    REQUIRE(q.size() == 0);
}

TEST_CASE("drain returns messages in FIFO order and clears", "[steer]") {
    SteerQueue q;
    q.enqueue("a");
    q.enqueue("b");
    q.enqueue("c");
    REQUIRE(q.size() == 3);

    const auto drained = q.drain();
    REQUIRE(drained == std::vector<std::string>{"a", "b", "c"});
    REQUIRE(q.empty());
    REQUIRE(q.drain().empty()); // second drain yields nothing
}
