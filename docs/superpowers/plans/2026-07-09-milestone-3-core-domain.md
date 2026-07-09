# Milestone 3: Core Domain — Implementation Plan

**Goal:** Introduce the orchestration domain — `Task`, `TaskGraph` (DAG), `Agent`/
`AgentManager`, `WorkspaceManager`, and the `Orchestrator` that advances the graph,
assigns agents, forwards upstream outputs into downstream prompts, and stores results.

**Architecture:** Two new libraries.
- `libs/orchestrator` (Qt-free, process-free, pure domain): all types above plus the
  `ITaskExecutor` seam (`ExecRequest` in, `TaskResult` out). Fully unit-testable with a
  fake executor — no real processes.
- `libs/runtime` (wiring, POSIX for now): `ProviderRegistry`, `ResultCollector`, and
  `ProcessTaskExecutor` that runs an `ExecRequest` through an `IProvider` +
  `ProcessManager` + `PosixProcessBackend` and collects chunks into a `TaskResult`.

This split is deliberate: the domain never knows how a task is executed, so the DAG,
dependency gating, failure propagation, and context-forwarding logic are testable in
isolation, and the executor is swappable (fake / process / future Qt).

## Types

- `TaskState`: Pending → Running → Succeeded | Failed | Blocked.
- `Task`: id, name, prompt, provider (ProviderId), dependencies, priority, state, output.
- `TaskGraph`: addTask/addDependency, readyTasks() (Pending with all deps Succeeded,
  priority-desc then id-asc), markRunning/Succeeded/Failed, failure propagates Blocked to
  transitive dependents, isComplete(), hasCycle().
- `ExecRequest{provider, prompt, resume?, cwd?}`, `TaskResult{success, output, session?, costUsd?}`.
- `ITaskExecutor::execute(ExecRequest) -> TaskResult` (synchronous in M3; parallelism is M4).
- `Agent{id,name,provider,status,currentTask?}`, `AgentStatus{Idle,Busy,Stopped}`.
- `AgentManager`: createAgent, findIdle(provider), setStatus.
- `WorkspaceManager`: putArtifact/getArtifact (in-memory map; persistence is M5).
- `Orchestrator`: run() → dispatch ready tasks, acquire an agent per provider, compose the
  effective prompt from dependency outputs, execute, mark result, store artifact, repeat
  until complete. Emits OrchestratorEvents; returns a RunReport.

## Context forwarding (the mediation rule)

`composePrompt(task)` = task.prompt + for each succeeded dependency:
`"\n\n## Context from step \"<depName>\":\n<depOutput>"`. Agents receive upstream results
only through this — never a direct channel.

## runtime lib

- `ProviderRegistry`: name → shared_ptr<IProvider>.
- `ResultCollector`: accumulate TaskChunks (assistant text, result text, stdout, session,
  cost, error) → TaskResult. Pure, unit-tested.
- `ProcessTaskExecutor`: registry + spawns PosixProcessBackend per execute, feeds stdout
  through NdjsonLineReader + provider.parseFrame + ResultCollector, returns TaskResult.

## Tests (TDD)

- test_task_graph: linear deps ordering, diamond, failure→blocked propagation, cycle detect,
  ready-set priority ordering, isComplete.
- test_agent_manager: create, findIdle by provider, status transitions.
- test_workspace: put/get/overwrite/absent.
- test_orchestrator (FakeTaskExecutor): executes in dependency order; forwards dep output
  into downstream prompt; failed dep blocks dependents (not executed); artifacts stored;
  agent status returns to Idle; RunReport counts.
- test_result_collector: assistant-only, result-overrides-assistant, stdout fallback,
  error flag, cost/session capture.
- integration/test_process_task_executor: real echo via GenericCliProvider returns output;
  missing provider → failure.

## Demo

`maestro-cli --graph "<topic>"`: 3-node DAG (research → draft → critique) via ClaudeProvider
+ Orchestrator + ProcessTaskExecutor, printing per-task status and forwarded context.
