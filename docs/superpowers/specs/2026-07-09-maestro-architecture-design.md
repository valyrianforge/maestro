# Maestro — AI Orchestration Platform: Architecture Design

**Status:** Approved architecture map (v1)
**Date:** 2026-07-09
**Scope of this document:** The top-level architecture, module responsibilities, folder
structure, data flow, concurrency model, testing strategy, and implementation roadmap.
This is the *map*. Each milestone below gets its own spec → plan → build cycle.

---

## 1. Product summary

Maestro is a cross-platform desktop application that orchestrates multiple **already-installed,
already-authenticated** AI coding CLIs (Claude Code, Codex, and others) as a fleet of
coordinated agents. Maestro itself requires **no cloud API keys** for primary operation — it
drives the local CLIs, which own their own auth.

The orchestrator is the brain. **AI tools never communicate directly with each other.** Every
interaction flows through the orchestrator, which mediates what information passes between agents
via a shared workspace.

## 2. Locked decisions

These were decided during brainstorming and are binding for v1:

| # | Decision | Choice |
|---|----------|--------|
| 1 | CLI drive model | **Headless per-task + session resume.** Each task is one `claude -p --output-format stream-json` (or equivalent) invocation. Long-lived "sessions" are simulated via `--resume` / `--session-id`. No PTY puppeting. |
| 2 | Concurrency model | **Pooled workers + priority task queue.** Hundreds of *logical* agents; a bounded pool (default = CPU cores) of *real* CLI processes leased per-invocation. |
| 3 | Target platforms | **Windows + macOS + Linux.** Process management via Qt `QProcess` (cross-platform) to avoid per-OS PTY code. |
| 4 | Orchestration | **General task-graph (DAG) engine.** The Planner→Architecture→Implementation→Testing→Review→Docs→Merge flow is one built-in graph *template*, not hard-coded control flow. |
| 5 | Testing | **TDD from day one.** Fakeable seams for every external dependency; Catch2; a checked-in fake-CLI for integration tests. |
| 6 | Core purity | **Qt-free Core Engine.** Core is a plain C++20/23 library with no Qt dependency. Qt lives only in the UI layer and in the `process` backend implementation. |
| 7 | Async style | **C++20 coroutines** for the spawn→stream→collect→resolve flow, over a thread-pool executor. |
| 8 | Test framework | **Catch2.** |
| 9 | Storage | **SQLite** initially, behind an `ISqlStore` interface. |

## 3. Guiding principles

- **The orchestrator owns all state; agents own none.** Agents are logical actors that
  produce/consume artifacts in the Workspace. They hold no channels to each other. This is what
  makes "hundreds of agents" tractable — they are cheap value objects, not threads/processes.
- **Logical agent ≠ OS process.** Potentially hundreds of `Agent` objects, but a bounded
  `ProcessPool` of real CLI processes. The scheduler *leases* a worker to a task for the duration
  of one invocation; agents never own processes.
- **Everything crosses a seam we can fake.** Every external dependency (CLI process, clock, DB,
  filesystem) sits behind an interface so tests run with zero real processes and no network.
- **Dependencies point inward.** UI → Core → Domain. Provider/Process/Storage/Plugin layers are
  plugged *into* Core via interfaces that Core defines. Core never `#include`s Qt.
- **One direction each across the UI seam.** Core emits immutable events (state deltas); the UI
  sends commands. No shared mutable state across the boundary.

## 4. Layered architecture

```
┌─────────────────────────────────────────────────────────────┐
│ UI LAYER (Qt6)                                                │
│  Project Explorer · Agent List · Task Queue · Terminal ·      │
│  Live Logs · Conversation Viewer · Resource Monitor ·         │
│  Settings · Plugin Manager · File Explorer                    │
│  — observes Core via a read-model + event bus, sends commands │
└───────────────▲───────────────────────────┬──────────────────┘
                │ events (state deltas)      │ commands
┌───────────────┴───────────────────────────▼──────────────────┐
│ CORE ENGINE (Orchestrator)  — no Qt                           │
│  Orchestrator · TaskGraph(DAG) · Scheduler · AgentManager ·   │
│  WorkspaceManager · MemoryStore · EventBus · CommandBus       │
└──┬────────────┬─────────────┬──────────────┬─────────────┬────┘
   │            │             │              │             │
┌──▼───┐  ┌─────▼──────┐ ┌────▼─────┐  ┌─────▼─────┐ ┌─────▼─────┐
│PROVI-│  │  PROCESS   │ │ STORAGE  │  │  MEMORY   │ │  PLUGIN   │
│DERS  │  │  MANAGER   │ │ (SQLite) │  │  backend  │ │  HOST     │
│      │  │+ProcessPool│ │          │  │           │ │           │
└──┬───┘  └─────┬──────┘ └──────────┘  └───────────┘ └─────┬─────┘
   │            │                                          │
   │      ┌─────▼──────────────────────┐            ┌──────▼──────┐
   │      │ real CLI child processes    │◄───────────│ 3rd-party   │
   └─────►│ (claude -p, codex, …)       │  provided  │ provider    │
          └─────────────────────────────┘  by plugin │ .so/.dll    │
                                                      └─────────────┘
```

### Data flow for one unit of work

```
UI command
  → Orchestrator (translate goal into / advance a TaskGraph)
  → TaskGraph node becomes ready (all deps satisfied)
  → Scheduler dequeues by priority
  → leases a worker from ProcessPool
  → Provider builds argv/session spec for the target CLI
  → ProcessManager spawns child (QtProcessBackend)
  → stdout/stderr streamed as events
  → Provider parses (stream-json) into structured TaskChunks
  → results written to Workspace + Memory + SQLite
  → dependent graph nodes unlock
  → events flow to UI (Terminal, Logs, Agent status, Task queue)
```

## 5. Folder structure

```
maestro/
├── CMakeLists.txt                 # top-level, options, find_package(Qt6)
├── cmake/                         # toolchain, warnings, sanitizers helpers
├── third_party/                   # vendored deps (or via vcpkg/FetchContent)
├── apps/
│   └── maestro-desktop/           # the Qt executable (main.cpp, DI wiring)
├── libs/
│   ├── core/                      # NO Qt. the engine.
│   │   ├── include/maestro/core/  # public headers (interfaces live here)
│   │   └── src/
│   │       ├── orchestrator/
│   │       ├── graph/             # TaskGraph, Task, dependency resolution
│   │       ├── scheduler/         # priority queue, worker leasing
│   │       ├── agent/             # Agent, AgentManager
│   │       ├── workspace/         # WorkspaceManager, artifact refs
│   │       ├── memory/            # MemoryStore facade + interfaces
│   │       └── events/            # EventBus, CommandBus, event types
│   ├── process/                   # ProcessManager, ProcessPool (Qt QProcess)
│   ├── providers/                 # IProvider + Claude/Codex/GenericCLI/LocalLLM
│   ├── storage/                   # ISqlStore + SQLite impl, migrations
│   ├── plugin/                    # PluginHost, C-ABI, plugin SDK headers
│   └── logging/                   # structured logger, sinks
├── plugins/                       # example out-of-tree providers
├── ui/                            # Qt widgets/QML, view-models, resources
└── tests/
    ├── unit/                      # core logic, fakes only
    ├── integration/               # real process against a scripted fake-CLI
    └── fakes/                     # FakeProvider, FakeProcessBackend, FakeClock
```

## 6. Module responsibilities & key interfaces

Signatures are illustrative; final ones are fixed in each milestone's spec.

### 6.1 Provider layer — the abstraction the scheduler talks to

```cpp
struct TaskRequest  { AgentId agent; std::string prompt; WorkspaceRef ws;
                      std::optional<SessionId> resume; json params; };
struct TaskChunk    { enum Kind{ Stdout, Stderr, Structured, Done, Error } kind;
                      std::string text; json data; };

class IProvider {                       // pure interface, defined in Core
public:
  virtual ~IProvider() = default;
  virtual ProviderId   id() const = 0;
  virtual Capabilities capabilities() const = 0;      // streaming? resume? cost?
  virtual ProcessSpec  buildSpec(const TaskRequest&) const = 0;   // argv/env/cwd
  virtual TaskChunk    parse(std::string_view rawFrame) const = 0;
  virtual std::optional<SessionId> extractSession(const TaskChunk&) const = 0;
};
```

- `ClaudeProvider` — emits `claude -p <prompt> --output-format stream-json --session-id …`,
  parses the JSON stream, extracts the session id for later resume.
- `CodexProvider`, `GenericCLIProvider` (template-configured argv + regex/JSON parse),
  `LocalLLMProvider`.
- **Why:** the scheduler only ever sees `IProvider`. Adding a tool = one class (or a plugin),
  zero core changes. Satisfies "the scheduler never touches provider-specific code."

### 6.2 Process layer

- `IProcessBackend` (interface) → `QtProcessBackend` (real, via `QProcess`) and
  `FakeProcessBackend` (tests). **This seam lets the whole engine be TDD'd with no real CLI.**
- `ProcessManager` — spawn, async stdout/stderr read, liveness detection, restart-on-failure
  policy, graceful kill/timeout.
- `ProcessPool` — bounded set of reusable workers; leases a worker to the scheduler and reclaims
  it. Enforces the concurrency cap; *is* the system's backpressure throttle.

### 6.3 Core Engine

- `Orchestrator` — top-level use-case coordinator; turns a user goal into a `TaskGraph`, owns
  lifecycle, mediates all agent I/O ("AI tools never talk directly" is enforced here).
- `TaskGraph` / `Task` — a DAG. `Task` carries id, deps, assigned provider/agent, state,
  priority, retry policy, artifact refs. The ready-set is computed from satisfied dependencies,
  enabling parallel branches.
- `Scheduler` — pulls ready tasks by priority, leases workers from `ProcessPool`, applies
  backpressure, handles retry / pause / resume and dependency gating.
- `AgentManager` / `Agent` — lightweight: id, provider ref, status, priority, current task,
  conversation + working-memory handles, logs, workspace ref.
- `WorkspaceManager` — the shared project state: source files, docs, build outputs, design notes,
  task-graph snapshot, agent notes, shared memory. Agents read/write artifacts here instead of
  messaging each other.
- `MemoryStore` — facade over Global / Project / Agent-local memory + task history + decision
  log; persisted (survives restart).
- `EventBus` / `CommandBus` — decouple UI from Core. Core emits immutable events (state deltas);
  UI sends commands. One direction each.

### 6.4 Storage

`ISqlStore` interface + SQLite implementation, schema-migrated. Tables:
`projects`, `sessions`, `tasks`, `task_edges`, `agents`, `agent_history`, `logs`, `memory`,
`config`. In-memory SQLite is used in unit tests.

### 6.5 Plugin host

`PluginHost` loads shared libraries exposing a **C ABI** factory
(`maestro_create_provider`) that returns an `IProvider`. A C ABI at the boundary (not C++ vtables
across `.so`/`.dll`) survives compiler/version differences. Adding a provider requires **no**
recompilation of the app.

### 6.6 Logging

Structured (key-value / JSON) logger with pluggable sinks: file, an in-memory ring buffer for the
UI Live Logs panel, and SQLite. Categories: process, scheduler, task, error, perf, resource.

## 7. Concurrency model

- Core runs an **async task loop** on a thread-pool executor. Scheduler logic is single-threaded
  over queues; heavy work (process I/O) is offloaded.
- **C++20 coroutines** express the "spawn → stream → collect → resolve" async flow so provider
  logic reads sequentially without callback nesting.
- Inter-thread and UI communication is **message-passing via the EventBus**, minimizing shared
  mutable state and locks. The UI marshals events onto the Qt main thread.
- **Backpressure:** the bounded `ProcessPool` is the throttle. Hundreds of ready tasks may queue;
  only N run at once. Protects CPU, RAM, and — critically — **API rate limits**.

## 8. Testing strategy (TDD from day one)

- **Unit:** graph resolution, scheduler ordering/retry, memory, provider argv-building and
  parsing — all against fakes. No processes, no DB files (in-memory SQLite).
- **Integration:** a checked-in **fake-CLI program** emits canned stream-json and is driven
  through the *real* `QtProcessBackend`, proving spawn/stream/parse/restart without spending API
  tokens.
- **Contract tests:** every `IProvider` runs the same shared suite, so a new provider cannot
  silently break the contract.
- **Framework:** Catch2.

## 9. Implementation roadmap (risk-ordered)

Each milestone is a separate spec → plan → build cycle with an approval gate.

0. **Skeleton** — CMake, three-platform build, empty libs, Catch2 wired, CI-ready.
1. **Process spine** — `IProcessBackend` + `QtProcessBackend` + `FakeProcessBackend`,
   `ProcessManager` (spawn/stream/restart). *Proves the load-bearing risk.*
2. **Provider contract** — `IProvider`, `GenericCLIProvider`, `ClaudeProvider` driving one real
   headless `claude -p` end-to-end.
3. **Core domain** — `Task`, `TaskGraph`, `AgentManager`, `WorkspaceManager`, `Orchestrator`
   (single agent, single task).
4. **Scheduler + ProcessPool** — priority queue, worker leasing, retry/pause, parallel branches.
5. **Persistence** — SQLite store + migrations; state survives restart.
6. **Memory system** — global / project / agent-local / history / decision-log.
7. **UI shell** — Qt IDE layout, event-bus wiring; Terminal/Logs/Agents/Tasks panels live.
8. **Multi-agent workflow** — the Planner→…→Merge graph template on the DAG engine.
9. **Plugin host** — C-ABI provider loading; port one provider to a plugin as proof.
10. **Polish** — resource monitor, settings, plugin manager UI, hardening.

## 10. Explicit non-goals for v1 (YAGNI)

- No PTY / interactive TUI puppeting (headless only).
- No cloud API keys managed by Maestro (CLIs own their auth).
- No distributed / multi-machine orchestration (single host).
- No provider auto-discovery beyond configured/known CLIs.
- No non-SQLite storage backends (interface allows it later).

## 11. Open questions deferred to milestone specs

- Exact SQLite schema DDL and migration mechanism (milestone 5).
- Coroutine executor: hand-rolled thread pool vs. a small library (milestone 1).
- Qt Widgets vs. QML for the UI shell (milestone 7).
- Concrete `stream-json` schema per CLI version and version-detection strategy (milestone 2).
