#pragma once
#include <functional>
#include <string>
#include <vector>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/acp/Plan.hpp"
#include "maestro/acp/PlanApprovalGate.hpp"
#include "maestro/acp/SessionController.hpp"

namespace maestro::acp {

// The supervisor loop. Watches the nucleus session's stream for `propose_plan`
// orchestration tool calls, routes each proposed fan-out through the plan
// approval gate, and — once a human approves — spawns the worker electrons and
// hands each its goal. This is the "nucleus proposes → you approve → electrons
// execute" flow, expressed as Qt-free Core logic.
class Nucleus {
public:
    // Creates a worker session for a spec and returns its controller (already
    // registered in the SessionManager as a child of the nucleus). Returns
    // nullptr to skip a worker.
    using Spawner = std::function<SessionController*(const WorkerSpec&)>;

    Nucleus(std::string nucleusSessionId, Spawner spawner);

    // Feed a session/update from the nucleus session. A `propose_plan` tool call
    // is extracted into a Plan and sent to the approval gate; other updates are
    // ignored here.
    void onNucleusUpdate(const SessionUpdate& update);

    [[nodiscard]] PlanApprovalGate& gate() { return gate_; }
    [[nodiscard]] const std::vector<std::string>& workerIds() const { return workerIds_; }
    [[nodiscard]] const std::string& nucleusSessionId() const { return nucleusSessionId_; }

private:
    void launch(const Plan& plan);

    std::string nucleusSessionId_;
    Spawner spawner_;
    PlanApprovalGate gate_;
    std::vector<std::string> workerIds_;
};

} // namespace maestro::acp
