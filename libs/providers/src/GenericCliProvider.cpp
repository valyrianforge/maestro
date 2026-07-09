#include "maestro/providers/GenericCliProvider.hpp"

namespace maestro::providers {
namespace {

std::string substitutePrompt(const std::string& templateArg, const std::string& prompt) {
    constexpr std::string_view token = "{{prompt}}";
    std::string result;
    std::size_t pos = 0;
    for (;;) {
        const std::size_t found = templateArg.find(token, pos);
        if (found == std::string::npos) {
            result.append(templateArg, pos, std::string::npos);
            break;
        }
        result.append(templateArg, pos, found - pos);
        result.append(prompt);
        pos = found + token.size();
    }
    return result;
}

} // namespace

ProcessSpec GenericCliProvider::buildSpec(const TaskRequest& request) const {
    ProcessSpec spec;
    spec.program = config_.program;
    spec.args.reserve(config_.argsTemplate.size());
    for (const auto& arg : config_.argsTemplate) {
        spec.args.push_back(substitutePrompt(arg, request.prompt));
    }
    spec.workingDirectory = request.workingDirectory;
    return spec;
}

std::optional<TaskChunk> GenericCliProvider::parseFrame(std::string_view frame) const {
    TaskChunk chunk;
    chunk.kind = TaskChunk::Kind::Stdout;
    chunk.text = std::string{frame};
    return chunk;
}

} // namespace maestro::providers
