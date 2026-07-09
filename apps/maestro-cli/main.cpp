// maestro-cli: a minimal console harness that drives a real child process
// through the ProcessManager + PosixProcessBackend, streaming its output live.
//
// Usage:
//   maestro-cli "your prompt here"     -> runs: claude -p "your prompt here"
//   maestro-cli --exec <cmd> [args...] -> runs an arbitrary command (safe smoke test)
//   maestro-cli                        -> runs claude with a default greeting prompt

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "maestro/process/PosixProcessBackend.hpp"
#include "maestro/process/ProcessManager.hpp"

using namespace maestro::core;
using namespace maestro::process;

namespace {

const char* reasonName(ExitReason r) {
    switch (r) {
    case ExitReason::Exited:  return "exited";
    case ExitReason::Killed:  return "killed";
    case ExitReason::Crashed: return "crashed";
    }
    return "unknown";
}

ProcessSpec specFromArgs(int argc, char** argv) {
    ProcessSpec spec;
    if (argc >= 2 && std::string(argv[1]) == "--exec") {
        if (argc < 3) {
            spec.program = "true"; // nothing to run
            return spec;
        }
        spec.program = argv[2];
        for (int i = 3; i < argc; ++i) {
            spec.args.emplace_back(argv[i]);
        }
        return spec;
    }

    std::string prompt;
    for (int i = 1; i < argc; ++i) {
        if (!prompt.empty()) prompt += ' ';
        prompt += argv[i];
    }
    if (prompt.empty()) {
        prompt = "Reply with a short one-sentence greeting.";
    }
    spec.program = "claude";
    spec.args = {"-p", prompt};
    return spec;
}

} // namespace

int main(int argc, char** argv) {
    const ProcessSpec spec = specFromArgs(argc, argv);

    std::cout << "maestro-cli :: launching \"" << spec.program << "\"";
    for (const auto& a : spec.args) {
        std::cout << ' ' << '"' << a << '"';
    }
    std::cout << "\n--------------------------------------------------\n";

    PosixProcessBackend backend;
    ProcessManager mgr(backend);

    bool done = false;
    ProcessExit result{ExitReason::Crashed, -1};
    const auto started = std::chrono::steady_clock::now();

    mgr.spawn(spec, RestartPolicy::none(),
              ProcessCallbacks{
                  [](std::string_view s) { std::cout << s << std::flush; },
                  [](std::string_view s) { std::cerr << s << std::flush; },
                  [&](ProcessExit e) { result = e; done = true; }});

    while (!done) {
        backend.processEvents(200);
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started);

    std::cout << "\n--------------------------------------------------\n";
    std::cout << "maestro-cli :: process " << reasonName(result.reason)
              << " (code " << result.exitCode << ") in " << elapsed.count() << " ms"
              << (result.succeeded() ? "  [OK]" : "  [FAIL]") << "\n";

    return result.succeeded() ? 0 : 1;
}
