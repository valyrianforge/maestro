#pragma once
#include <string>
#include <vector>

#include "maestro/core/Provider.hpp"

namespace maestro::providers {

using core::Capabilities;
using core::IProvider;
using core::ProcessSpec;
using core::ProviderId;
using core::TaskChunk;
using core::TaskRequest;

// A configuration-driven provider for arbitrary line-oriented CLIs. The launch
// command is a template; the literal token "{{prompt}}" in any argument is
// replaced with the request prompt. Output is treated as plain text: each line
// becomes a Stdout chunk. This is how new tools get added with zero code.
class GenericCliProvider final : public IProvider {
public:
    struct Config {
        ProviderId id;
        std::string program;
        std::vector<std::string> argsTemplate; // may contain "{{prompt}}" tokens
        Capabilities capabilities;
    };

    explicit GenericCliProvider(Config config) : config_(std::move(config)) {}

    [[nodiscard]] ProviderId id() const override { return config_.id; }
    [[nodiscard]] Capabilities capabilities() const override { return config_.capabilities; }
    [[nodiscard]] ProcessSpec buildSpec(const TaskRequest& request) const override;
    [[nodiscard]] std::optional<TaskChunk> parseFrame(std::string_view frame) const override;

private:
    Config config_;
};

} // namespace maestro::providers
