// maestro-cli: a console harness that drives a real child process through the
// ProcessManager + PosixProcessBackend, streaming output live.
//
// Usage:
//   maestro-cli "your prompt here"     -> ClaudeProvider: claude -p ... stream-json
//   maestro-cli --exec <cmd> [args...] -> run an arbitrary command (raw passthrough)
//   maestro-cli                        -> ClaudeProvider with a default prompt

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "maestro/orchestrator/Orchestrator.hpp"
#include "maestro/providers/ClaudeProvider.hpp"
#include "maestro/providers/NdjsonLineReader.hpp"
#include "maestro/process/PosixProcessBackend.hpp"
#include "maestro/process/ProcessManager.hpp"
#include "maestro/runtime/ProcessTaskExecutor.hpp"
#include "maestro/runtime/ProviderRegistry.hpp"
#include "maestro/runtime/RunRecorder.hpp"
#include "maestro/storage/SqliteStore.hpp"

using namespace maestro::core;
using namespace maestro::process;
using maestro::providers::ClaudeProvider;
using maestro::providers::NdjsonLineReader;
namespace orch = maestro::orchestrator;
namespace rt = maestro::runtime;
namespace st = maestro::storage;

namespace {

// Location of the on-disk history database: ~/.maestro/maestro.db
std::string dbPath() {
    const char* home = std::getenv("HOME");
    std::filesystem::path dir = std::filesystem::path(home ? home : ".") / ".maestro";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return (dir / "maestro.db").string();
}

// Opens the store and creates a run row, returning (store, runId). The caller
// attaches a RunRecorder, runs, then snapshots + finishes.
struct RunContext {
    std::unique_ptr<st::SqliteStore> store;
    std::int64_t runId{0};
};

RunContext beginRun(const std::string& topic, const std::string& mode) {
    RunContext ctx;
    ctx.store = std::make_unique<st::SqliteStore>(dbPath());
    std::int64_t projectId = 0;
    if (const auto existing = ctx.store->getConfig("cli_project_id")) {
        projectId = std::stoll(*existing);
    } else {
        projectId = ctx.store->createProject("cli", ".", st::SqliteStore::isoNow());
        ctx.store->setConfig("cli_project_id", std::to_string(projectId));
    }
    ctx.runId = ctx.store->createRun(projectId, topic, mode, st::SqliteStore::isoNow());
    return ctx;
}

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

// Runs a fixed 3-agent pipeline over one topic to demonstrate real
// orchestration: research -> draft -> critique, with the orchestrator forwarding
// each step's output into the next step's prompt automatically.
int runGraph(const std::string& topic) {
    rt::ProviderRegistry registry;
    registry.add(std::make_shared<ClaudeProvider>());

    orch::TaskGraph graph;
    orch::Task research;
    research.name = "research";
    research.provider = ProviderId{"claude"};
    research.prompt = "List 3 concise, factual bullet points about: " + topic;
    const auto researchId = graph.addTask(research);

    orch::Task draft;
    draft.name = "draft";
    draft.provider = ProviderId{"claude"};
    draft.prompt = "Using the research context, write a 3-sentence explanation of: " + topic;
    const auto draftId = graph.addTask(draft);
    graph.addDependency(draftId, researchId);

    orch::Task critique;
    critique.name = "critique";
    critique.provider = ProviderId{"claude"};
    critique.prompt = "Critique the draft above for accuracy and clarity in 2 short bullets.";
    const auto critiqueId = graph.addTask(critique);
    graph.addDependency(critiqueId, draftId);

    orch::WorkspaceManager workspace;
    orch::AgentManager agents;

    rt::ProcessTaskExecutor executor(
        registry, []() { return std::make_unique<PosixProcessBackend>(); },
        [](const orch::ExecRequest&, std::string_view text) {
            std::cout << text << std::flush;
        });

    RunContext ctx = beginRun(topic, "pipeline");
    rt::RunRecorder recorder(*ctx.store, ctx.runId, graph);
    auto record = recorder.observer();

    orch::Orchestrator orchestrator(graph, executor, workspace, agents);
    orchestrator.setObserver([&](const orch::OrchestratorEvent& e) {
        record(e);
        using T = orch::OrchestratorEvent::Type;
        const std::string& name = graph.at(e.task).name;
        if (e.type == T::TaskStarted) {
            std::cout << "\n\n=== [" << name << "] running (agent " << e.agent.value()
                      << ") ===\n";
        } else if (e.type == T::TaskSucceeded) {
            std::cout << "\n--- [" << name << "] done ---\n";
        } else if (e.type == T::TaskFailed) {
            std::cout << "\n!!! [" << name << "] FAILED: " << e.detail << "\n";
        }
    });

    std::cout << "maestro-cli :: graph pipeline on topic: \"" << topic << "\"\n";
    const orch::RunReport report = orchestrator.run();

    recorder.snapshot(agents);
    ctx.store->finishRun(ctx.runId, st::SqliteStore::isoNow(), report.succeeded, report.failed,
                         report.blocked);

    std::cout << "\n==================================================\n"
              << "pipeline complete: " << report.succeeded << " succeeded, " << report.failed
              << " failed, " << report.blocked << " blocked\n"
              << "saved as run #" << ctx.runId << " (see: maestro-cli --history)\n";
    return report.failed == 0 ? 0 : 1;
}

// Runs N independent Claude tasks concurrently to demonstrate parallelism:
// wall-clock stays close to a single call rather than N sequential calls.
int runFan(int n, const std::string& prompt) {
    rt::ProviderRegistry registry;
    registry.add(std::make_shared<ClaudeProvider>());

    orch::TaskGraph graph;
    for (int i = 0; i < n; ++i) {
        orch::Task t;
        t.name = "worker-" + std::to_string(i + 1);
        t.provider = ProviderId{"claude"};
        t.prompt = prompt + " (answer in one short sentence)";
        graph.addTask(t);
    }

    orch::WorkspaceManager workspace;
    orch::AgentManager agents;
    rt::ProcessTaskExecutor executor(
        registry, []() { return std::make_unique<PosixProcessBackend>(); });

    orch::Orchestrator orchestrator(graph, executor, workspace, agents,
                                    orch::SchedulerConfig{/*maxConcurrency=*/n, /*retries=*/0});

    RunContext ctx = beginRun(prompt, "fan");
    rt::RunRecorder recorder(*ctx.store, ctx.runId, graph);
    auto record = recorder.observer();

    const auto start = std::chrono::steady_clock::now();
    auto sinceStart = [&] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    };

    orchestrator.setObserver([&](const orch::OrchestratorEvent& e) {
        record(e);
        using T = orch::OrchestratorEvent::Type;
        const std::string& name = graph.at(e.task).name;
        if (e.type == T::TaskStarted) {
            std::cout << "  [+" << sinceStart() << "ms] " << name << " started\n" << std::flush;
        } else if (e.type == T::TaskSucceeded) {
            std::cout << "  [+" << sinceStart() << "ms] " << name << " done\n" << std::flush;
        }
    });

    std::cout << "maestro-cli :: fan-out " << n << " concurrent Claude agents\n"
              << "--------------------------------------------------\n";
    const orch::RunReport report = orchestrator.run();

    recorder.snapshot(agents);
    ctx.store->finishRun(ctx.runId, st::SqliteStore::isoNow(), report.succeeded, report.failed,
                         report.blocked);

    std::cout << "--------------------------------------------------\n"
              << report.succeeded << "/" << n << " succeeded in " << sinceStart()
              << " ms wall-clock (concurrency " << n << ")\n"
              << "saved as run #" << ctx.runId << " (see: maestro-cli --history)\n";
    return report.failed == 0 ? 0 : 1;
}

int showHistory() {
    st::SqliteStore store(dbPath());
    const auto runs = store.listRuns(50);
    if (runs.empty()) {
        std::cout << "no runs yet. Try: maestro-cli --graph \"your topic\"\n";
        return 0;
    }
    std::cout << "maestro-cli :: run history (" << runs.size() << ")\n";
    std::cout << "--------------------------------------------------\n";
    for (const auto& r : runs) {
        std::cout << "#" << r.id << "  [" << r.mode << "]  " << r.startedAt << "  "
                  << r.succeeded << "ok/" << r.failed << "fail/" << r.blocked << "blk  \""
                  << r.topic.substr(0, 60) << "\"\n";
    }
    std::cout << "\ninspect one with: maestro-cli --show <id>\n";
    return 0;
}

int showRun(std::int64_t runId) {
    st::SqliteStore store(dbPath());
    const auto run = store.getRun(runId);
    if (!run) {
        std::cout << "no run #" << runId << "\n";
        return 1;
    }
    std::cout << "run #" << run->id << "  [" << run->mode << "]  \"" << run->topic << "\"\n";
    std::cout << "started " << run->startedAt << "  finished " << run->finishedAt << "\n\n";

    std::cout << "TASKS:\n";
    for (const auto& t : store.tasksForRun(runId)) {
        std::cout << "  - " << t.name << " (" << t.provider << ") [" << t.state << "]  "
                  << t.output.size() << " bytes output\n";
    }
    std::cout << "\nMESSAGE FLOW (forwarded context):\n";
    const auto edges = store.edgesForRun(runId);
    if (edges.empty()) {
        std::cout << "  (independent tasks; no forwarding)\n";
    }
    for (const auto& e : edges) {
        std::cout << "  " << e.prerequisite << "  --output-->  " << e.dependent << "\n";
    }
    std::cout << "\nEVENT TIMELINE:\n";
    for (const auto& ev : store.eventsForRun(runId)) {
        std::cout << "  [" << ev.seq << "] " << ev.ts << "  " << ev.type << "  " << ev.taskName
                  << (ev.agentId ? "  (agent " + std::to_string(ev.agentId) + ")" : "") << "\n";
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--history") {
        return showHistory();
    }
    if (argc >= 3 && std::string(argv[1]) == "--show") {
        return showRun(std::atoll(argv[2]));
    }
    if (argc >= 3 && std::string(argv[1]) == "--fan") {
        const int n = std::max(1, std::atoi(argv[2]));
        std::string prompt;
        for (int i = 3; i < argc; ++i) {
            if (!prompt.empty()) prompt += ' ';
            prompt += argv[i];
        }
        if (prompt.empty()) {
            prompt = "Name one interesting fact about the number " + std::to_string(n);
        }
        return runFan(n, prompt);
    }
    if (argc >= 2 && std::string(argv[1]) == "--graph") {
        std::string topic;
        for (int i = 2; i < argc; ++i) {
            if (!topic.empty()) topic += ' ';
            topic += argv[i];
        }
        if (topic.empty()) {
            topic = "directed acyclic graphs in build systems";
        }
        return runGraph(topic);
    }

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
