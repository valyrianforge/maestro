#include "maestro/acp/Plan.hpp"

namespace maestro::acp {

Plan parsePlanProposal(const Json& input) {
    Plan plan;
    if (!input.contains("workers") || !input["workers"].is_array()) {
        return plan;
    }
    for (const auto& w : input["workers"]) {
        WorkerSpec spec;
        if (w.contains("role") && w["role"].is_string()) {
            spec.role = w["role"].get<std::string>();
        }
        if (w.contains("goal") && w["goal"].is_string()) {
            spec.goal = w["goal"].get<std::string>();
        }
        if (w.contains("files") && w["files"].is_array()) {
            for (const auto& f : w["files"]) {
                if (f.is_string()) {
                    spec.files.push_back(f.get<std::string>());
                }
            }
        }
        plan.workers.push_back(std::move(spec));
    }
    return plan;
}

} // namespace maestro::acp
