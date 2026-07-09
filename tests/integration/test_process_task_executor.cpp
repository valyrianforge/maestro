// Integration: the real ProcessTaskExecutor running actual system commands via
// a GenericCliProvider (no network, no Claude tokens).
#include <catch2/catch_test_macros.hpp>

#include <memory>

#include "maestro/orchestrator/Orchestrator.hpp"
#include "maestro/process/PosixProcessBackend.hpp"
#include "maestro/providers/GenericCliProvider.hpp"
#include "maestro/runtime/ProcessTaskExecutor.hpp"
#include "maestro/runtime/ProviderRegistry.hpp"

using namespace maestro::core;
using namespace maestro::orchestrator;
using namespace maestro::runtime;
using maestro::providers::GenericCliProvider;

namespace {
std::shared_ptr<GenericCliProvider> echoProvider() {
    return std::make_shared<GenericCliProvider>(GenericCliProvider::Config{
        ProviderId{"echo"}, "echo", {"{{prompt}}"}, Capabilities{}});
}
} // namespace

TEST_CASE("executor runs a real command and returns its output", "[integration]") {
    ProviderRegistry registry;
    registry.add(echoProvider());
    ProcessTaskExecutor executor(
        registry, []() { return std::make_unique<maestro::process::PosixProcessBackend>(); });

    ExecRequest req;
    req.provider = ProviderId{"echo"};
    req.prompt = "orchestrated-hello";
    const TaskResult result = executor.execute(req);

    REQUIRE(result.success);
    REQUIRE(result.output == "orchestrated-hello");
}

TEST_CASE("executor reports failure for an unregistered provider", "[integration]") {
    ProviderRegistry registry;
    ProcessTaskExecutor executor(
        registry, []() { return std::make_unique<maestro::process::PosixProcessBackend>(); });

    ExecRequest req;
    req.provider = ProviderId{"ghost"};
    req.prompt = "x";
    const TaskResult result = executor.execute(req);

    REQUIRE_FALSE(result.success);
    REQUIRE(result.output.find("no provider") != std::string::npos);
}

TEST_CASE("a full graph runs end-to-end through the real executor", "[integration]") {
    ProviderRegistry registry;
    registry.add(echoProvider());
    ProcessTaskExecutor executor(
        registry, []() { return std::make_unique<maestro::process::PosixProcessBackend>(); });

    TaskGraph g;
    Task a;
    a.name = "a";
    a.prompt = "first";
    a.provider = ProviderId{"echo"};
    const auto aId = g.addTask(a);
    Task b;
    b.name = "b";
    b.prompt = "second";
    b.provider = ProviderId{"echo"};
    const auto bId = g.addTask(b);
    g.addDependency(bId, aId);

    WorkspaceManager ws;
    AgentManager agents;
    Orchestrator orch(g, executor, ws, agents);
    const RunReport report = orch.run();

    REQUIRE(report.succeeded == 2);
    REQUIRE(ws.getArtifact("task/a/output") == "first");
    // b's echoed prompt includes the forwarded context from a.
    const auto bOut = ws.getArtifact("task/b/output");
    REQUIRE(bOut.has_value());
    REQUIRE(bOut->find("first") != std::string::npos);
}
