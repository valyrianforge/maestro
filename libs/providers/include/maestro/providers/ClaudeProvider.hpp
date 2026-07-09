#pragma once
#include "maestro/core/Provider.hpp"

namespace maestro::providers {

using core::Capabilities;
using core::IProvider;
using core::ProcessSpec;
using core::ProviderId;
using core::TaskChunk;
using core::TaskRequest;

// Drives the Claude Code CLI in headless mode. Launches:
//   claude -p <prompt> --output-format stream-json --verbose [--resume <id>]
// and parses the NDJSON event stream (system/assistant/result/...) into chunks.
//
// Verified against claude CLI 2.1.x: stream-json in print mode REQUIRES
// --verbose, session_id is a UUID string, and the answer text lives in
// assistant frames' message.content[] text blocks, echoed in the final result.
class ClaudeProvider final : public IProvider {
public:
    explicit ClaudeProvider(std::string executable = "claude")
        : executable_(std::move(executable)) {}

    [[nodiscard]] ProviderId id() const override { return ProviderId{"claude"}; }
    [[nodiscard]] Capabilities capabilities() const override {
        return Capabilities{/*streaming=*/true, /*sessionResume=*/true, /*reportsCost=*/true};
    }
    [[nodiscard]] ProcessSpec buildSpec(const TaskRequest& request) const override;
    [[nodiscard]] std::optional<TaskChunk> parseFrame(std::string_view frame) const override;

private:
    std::string executable_;
};

} // namespace maestro::providers
