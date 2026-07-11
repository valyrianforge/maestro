#pragma once
#include <string>
#include <vector>

#include "maestro/acp/JsonRpc.hpp" // for Json

namespace maestro::acp {

// One worker (electron) the nucleus proposes to spawn.
struct WorkerSpec {
    std::string role;               // e.g. "implement auth", "write tests"
    std::string goal;               // the instruction handed to the worker
    std::vector<std::string> files; // files/areas the worker is scoped to (advisory)
};

// A fan-out proposal: the set of workers the nucleus wants to launch as a unit.
// The human approves/edits/rejects this whole thing before anything runs.
struct Plan {
    std::vector<WorkerSpec> workers;

    [[nodiscard]] bool empty() const { return workers.empty(); }
    [[nodiscard]] std::size_t size() const { return workers.size(); }
};

// Extract a Plan from the input object of a `propose_plan` orchestration tool
// call, i.e. { "workers": [ { "role", "goal", "files": [...] }, ... ] }.
// Tolerant of missing/mistyped fields (they default to empty).
Plan parsePlanProposal(const Json& input);

} // namespace maestro::acp
