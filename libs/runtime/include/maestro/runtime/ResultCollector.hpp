#pragma once
#include <optional>
#include <string>

#include "maestro/core/Provider.hpp"
#include "maestro/orchestrator/Execution.hpp"

namespace maestro::runtime {

using core::TaskChunk;
using orchestrator::TaskResult;

// Accumulates parsed TaskChunks (and raw stderr) from one provider run into a
// single TaskResult. Pure and process-free, so it is unit-tested directly.
//
// Output precedence on finalize: the terminal Result text if present, else the
// concatenated assistant text, else raw stdout (for non-structured tools).
class ResultCollector {
public:
    void add(const TaskChunk& chunk);
    void addStderr(std::string_view text);

    [[nodiscard]] TaskResult finalize(bool processSucceeded) const;

private:
    std::string assistantText_;
    std::string resultText_;
    std::string stdoutText_;
    std::string stderrText_;
    std::optional<core::SessionId> session_;
    std::optional<double> costUsd_;
    bool sawResult_{false};
    bool error_{false};
};

} // namespace maestro::runtime
