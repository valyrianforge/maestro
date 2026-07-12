// A minimal, deterministic ACP agent for integration tests. Speaks just enough
// of the Agent Client Protocol over stdio to prove the real transport works:
//   initialize      -> { protocolVersion: 1 }
//   session/new     -> { sessionId: "s1" }
//   session/prompt  -> streams one agent_message_chunk, then { stopReason: end_turn }
// No network, no tokens. Exits cleanly on stdin EOF.
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

void emit(const json& j) { std::cout << j.dump() << "\n" << std::flush; }

} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        const json j = json::parse(line, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded() || !j.contains("method") || !j["method"].is_string()) {
            continue;
        }
        const std::string method = j["method"].get<std::string>();
        const json id = j.contains("id") ? j["id"] : json();

        if (method == "initialize") {
            emit({{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"protocolVersion", 1}}}});
        } else if (method == "session/new") {
            emit({{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"sessionId", "s1"}}}});
        } else if (method == "session/prompt") {
            emit({{"jsonrpc", "2.0"},
                  {"method", "session/update"},
                  {"params",
                   {{"sessionId", "s1"},
                    {"update",
                     {{"sessionUpdate", "agent_message_chunk"},
                      {"content", {{"text", "hello from fake agent"}}}}}}}});
            emit({{"jsonrpc", "2.0"}, {"id", id}, {"result", {{"stopReason", "end_turn"}}}});
        }
        // Unknown methods are ignored (a real agent would error).
    }
    return 0;
}
