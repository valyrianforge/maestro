#include "maestro/acp/Nucleus.hpp"

#include <utility>

namespace maestro::acp {

Nucleus::Nucleus(std::string nucleusSessionId, Spawner spawner)
    : nucleusSessionId_(std::move(nucleusSessionId)), spawner_(std::move(spawner)) {
    gate_.onApproved = [this](const Plan& plan) { launch(plan); };
}

void Nucleus::onNucleusUpdate(const SessionUpdate& update) {
    if (update.kind != SessionUpdate::Kind::ToolCall) {
        return;
    }
    const Json& data = update.data;
    if (!data.contains("title") || !data["title"].is_string() ||
        data["title"].get<std::string>() != "propose_plan") {
        return;
    }
    const Json input = data.contains("rawInput") ? data["rawInput"] : Json::object();
    gate_.propose(parsePlanProposal(input));
}

void Nucleus::launch(const Plan& plan) {
    for (const auto& spec : plan.workers) {
        SessionController* worker = spawner_ ? spawner_(spec) : nullptr;
        if (worker == nullptr) {
            continue;
        }
        worker->send(spec.goal);
        workerIds_.push_back(worker->sessionId());
    }
}

} // namespace maestro::acp
