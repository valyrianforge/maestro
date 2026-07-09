// Contract suite: invariants EVERY IProvider must satisfy, run against each
// concrete provider. A new provider that violates these fails here.
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

#include "maestro/providers/ClaudeProvider.hpp"
#include "maestro/providers/GenericCliProvider.hpp"

using namespace maestro::core;
using maestro::providers::ClaudeProvider;
using maestro::providers::GenericCliProvider;

namespace {
std::vector<std::shared_ptr<IProvider>> allProviders() {
    return {
        std::make_shared<ClaudeProvider>(),
        std::make_shared<GenericCliProvider>(GenericCliProvider::Config{
            ProviderId{"generic"}, "tool", {"{{prompt}}"}, Capabilities{}}),
    };
}
} // namespace

TEST_CASE("every provider has a non-empty id", "[contract]") {
    for (const auto& p : allProviders()) {
        REQUIRE_FALSE(p->id().name.empty());
    }
}

TEST_CASE("every provider builds a spec with a program set", "[contract]") {
    TaskRequest req;
    req.prompt = "do the thing";
    for (const auto& p : allProviders()) {
        const ProcessSpec spec = p->buildSpec(req);
        REQUIRE_FALSE(spec.program.empty());
    }
}

TEST_CASE("every provider's parseFrame never throws on junk", "[contract]") {
    for (const auto& p : allProviders()) {
        REQUIRE_NOTHROW(p->parseFrame(""));
        REQUIRE_NOTHROW(p->parseFrame("garbage \x01\x02 not json"));
        REQUIRE_NOTHROW(p->parseFrame("{\"partial\":"));
    }
}
