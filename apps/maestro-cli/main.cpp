// maestro-cli: a console harness that drives a real child process through the
// ProcessManager + PosixProcessBackend, streaming output live.
//
// Usage:
//   maestro-cli "your prompt here"     -> ClaudeProvider: claude -p ... stream-json
//   maestro-cli --exec <cmd> [args...] -> run an arbitrary command (raw passthrough)
//   maestro-cli                        -> ClaudeProvider with a default prompt

#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "maestro/providers/ClaudeProvider.hpp"
#include "maestro/providers/NdjsonLineReader.hpp"
#include "maestro/process/PosixProcessBackend.hpp"
#include "maestro/process/ProcessManager.hpp"

using namespace maestro::core;
using namespace maestro::process;
using maestro::providers::ClaudeProvider;
using maestro::providers::NdjsonLineReader;

namespace {

const char* reasonName(ExitReason r) {
    switch (r) {
    case ExitReason::Exited:  return "exited";
    case ExitReason::Killed:  return "killed";
    case ExitReason::Crashed: return "crashed";
    }
    return "unknown";
}

int runRaw(const ProcessSpec& spec) {
    PosixProcessBackend backend;
    ProcessManager mgr(backend);
    bool done = false;
    ProcessExit result{ExitReason::Crashed, -1};

    mgr.spawn(spec, RestartPolicy::none(),
              ProcessCallbacks{[](std::string_view s) { std::cout << s << std::flush; },
                               [](std::string_view s) { std::cerr << s << std::flush; },
                               [&](ProcessExit e) { result = e; done = true; }});
    while (!done) {
        backend.processEvents(200);
    }
    std::cout << "\n[" << reasonName(result.reason) << " code " << result.exitCode << "]\n";
    return result.succeeded() ? 0 : 1;
}

// Drives a request through a provider, rendering only the assistant answer and
// a parsed summary (session id + cost) — the point of the provider layer.
int runWithClaude(const std::string& prompt) {
    ClaudeProvider provider;
    TaskRequest req;
    req.prompt = prompt;
    const ProcessSpec spec = provider.buildSpec(req);

    std::cout << "maestro-cli :: " << provider.id().name << " :: \"" << prompt << "\"\n"
              << "--------------------------------------------------\n";

    PosixProcessBackend backend;
    ProcessManager mgr(backend);
    NdjsonLineReader reader;

    bool done = false;
    ProcessExit exitStatus{ExitReason::Crashed, -1};
    std::optional<SessionId> session;
    std::optional<double> costUsd;
    bool sawError = false;

    auto handleLine = [&](const std::string& line) {
        const auto chunk = provider.parseFrame(line);
        if (!chunk) {
            return;
        }
        if (chunk->session) {
            session = chunk->session;
        }
        switch (chunk->kind) {
        case TaskChunk::Kind::AssistantText:
            std::cout << chunk->text << std::flush;
            break;
        case TaskChunk::Kind::Result:
            costUsd = chunk->costUsd;
            break;
        case TaskChunk::Kind::Error:
            sawError = true;
            std::cerr << "\n[provider error] " << chunk->text << "\n";
            break;
        default:
            break; // SessionStarted/Other: metadata only
        }
    };

    const auto started = std::chrono::steady_clock::now();
    mgr.spawn(spec, RestartPolicy::none(),
              ProcessCallbacks{
                  [&](std::string_view s) {
                      for (const auto& line : reader.feed(s)) {
                          handleLine(line);
                      }
                  },
                  [](std::string_view s) { std::cerr << s << std::flush; },
                  [&](ProcessExit e) { exitStatus = e; done = true; }});
    while (!done) {
        backend.processEvents(200);
    }
    if (const auto tail = reader.flush()) {
        handleLine(*tail);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);

    std::cout << "\n--------------------------------------------------\n"
              << "maestro-cli :: process " << reasonName(exitStatus.reason) << " (code "
              << exitStatus.exitCode << ") in " << elapsed.count() << " ms\n";
    if (session) {
        std::cout << "  session : " << session->value() << "  (resumable)\n";
    }
    if (costUsd) {
        std::cout << "  cost    : $" << *costUsd << "\n";
    }
    const bool ok = exitStatus.succeeded() && !sawError;
    std::cout << (ok ? "  status  : OK\n" : "  status  : FAIL\n");
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--exec") {
        ProcessSpec spec;
        if (argc < 3) {
            std::cerr << "usage: maestro-cli --exec <program> [args...]\n";
            return 2;
        }
        spec.program = argv[2];
        for (int i = 3; i < argc; ++i) {
            spec.args.emplace_back(argv[i]);
        }
        std::cout << "maestro-cli :: exec :: \"" << spec.program << "\"\n"
                  << "--------------------------------------------------\n";
        return runRaw(spec);
    }

    std::string prompt;
    for (int i = 1; i < argc; ++i) {
        if (!prompt.empty()) prompt += ' ';
        prompt += argv[i];
    }
    if (prompt.empty()) {
        prompt = "Reply with a short one-sentence greeting.";
    }
    return runWithClaude(prompt);
}
