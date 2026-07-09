#include "maestro/providers/ClaudeProvider.hpp"

#include <nlohmann/json.hpp>

namespace maestro::providers {
namespace {

using json = nlohmann::json;

std::optional<core::SessionId> readSession(const json& j) {
    if (auto it = j.find("session_id"); it != j.end() && it->is_string()) {
        std::string s = it->get<std::string>();
        if (!s.empty()) {
            return core::SessionId{std::move(s)};
        }
    }
    return std::nullopt;
}

// Concatenate the text of all {type:"text"} blocks in an assistant message.
std::string joinAssistantText(const json& message) {
    std::string out;
    const auto content = message.find("content");
    if (content == message.end() || !content->is_array()) {
        return out;
    }
    for (const auto& block : *content) {
        if (block.value("type", std::string{}) == "text" && block.contains("text") &&
            block["text"].is_string()) {
            out += block["text"].get<std::string>();
        }
    }
    return out;
}

} // namespace

ProcessSpec ClaudeProvider::buildSpec(const TaskRequest& request) const {
    ProcessSpec spec;
    spec.program = executable_;
    spec.args = {"-p", request.prompt, "--output-format", "stream-json", "--verbose"};
    if (request.resume && request.resume->valid()) {
        spec.args.push_back("--resume");
        spec.args.push_back(request.resume->value());
    }
    spec.workingDirectory = request.workingDirectory;
    return spec;
}

std::optional<TaskChunk> ClaudeProvider::parseFrame(std::string_view frame) const {
    // Tolerate blanks and non-JSON noise without throwing.
    json j = json::parse(frame, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) {
        return std::nullopt;
    }

    const std::string type = j.value("type", std::string{});
    const auto session = readSession(j);

    if (type == "assistant") {
        TaskChunk chunk;
        chunk.kind = TaskChunk::Kind::AssistantText;
        chunk.session = session;
        if (auto msg = j.find("message"); msg != j.end() && msg->is_object()) {
            chunk.text = joinAssistantText(*msg);
        }
        return chunk;
    }

    if (type == "result") {
        TaskChunk chunk;
        chunk.kind = TaskChunk::Kind::Result;
        chunk.session = session;
        chunk.text = j.value("result", std::string{});
        chunk.isError = j.value("is_error", false);
        if (auto c = j.find("total_cost_usd"); c != j.end() && c->is_number()) {
            chunk.costUsd = c->get<double>();
        }
        if (chunk.isError) {
            chunk.kind = TaskChunk::Kind::Error;
        }
        return chunk;
    }

    if (type == "system" && j.value("subtype", std::string{}) == "init") {
        TaskChunk chunk;
        chunk.kind = TaskChunk::Kind::SessionStarted;
        chunk.session = session;
        return chunk;
    }

    // system/hook_*, rate_limit_event, and anything else: recognized but not
    // content. Still surface the session if we learned one.
    TaskChunk chunk;
    chunk.kind = TaskChunk::Kind::Other;
    chunk.session = session;
    return chunk;
}

} // namespace maestro::providers
