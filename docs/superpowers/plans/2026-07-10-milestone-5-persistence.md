# Milestone 5: Persistence (SQLite) — Implementation Plan

**Goal:** Persist projects, runs, tasks, dependency edges, agents, and the full event
(message) stream to SQLite so work survives restarts — and so the future agent-graph /
conversation view (M6) can replay exactly who did what and which outputs were forwarded
between agents.

**Architecture:** A Qt-free `libs/storage` wraps SQLite (via SQLiteCpp, which bundles
sqlite3 — cross-platform incl. Windows). It exposes a plain repository (`SqliteStore`) with
typed record structs; it depends on nothing in our stack (no orchestrator, no Qt), so it is
unit-testable against an in-memory `:memory:` database. The wiring that turns live
orchestration into rows (`RunRecorder`) lives in `libs/runtime`, which already depends on
the orchestrator; it translates `OrchestratorEvent`s into event rows and snapshots the final
graph (tasks + edges + outputs). Edges + task outputs together ARE the message flow: an edge
B←A with A.output = "…" means "A sent this to B", which is what the viz renders.

## Schema (created on open; idempotent)

- `projects(id, name, root_path, created_at)`
- `runs(id, project_id, topic, mode, started_at, finished_at, succeeded, failed, blocked)`
- `tasks(id, run_id, name, provider, state, priority, output)`
- `task_edges(run_id, dependent, prerequisite)`   -- the forwarded-context edges
- `agents(id, run_id, name, provider)`
- `events(id, run_id, seq, ts, type, task_name, agent_id, detail)`  -- the live timeline
- `config(key PRIMARY KEY, value)`

## storage API (SqliteStore)

- ctor(path) — opens/creates DB and runs migrations. `:memory:` for tests.
- createProject(name, rootPath, createdAt) -> id
- createRun(projectId, topic, mode, startedAt) -> id
- saveTask(runId, {name, provider, state, priority, output})
- saveEdge(runId, dependent, prerequisite)
- saveAgent(runId, {name, provider})
- appendEvent(runId, {seq, ts, type, taskName, agentId, detail})
- finishRun(runId, finishedAt, succeeded, failed, blocked)
- listRuns(limit) -> [RunRecord]; tasksForRun/edgesForRun/eventsForRun/agentsForRun
- setConfig/getConfig
All writes wrapped in transactions where batched.

## runtime wiring (RunRecorder)

- ctor(SqliteStore&, runId): observer() returns a std::function<void(OrchestratorEvent)> that
  appends event rows (monotonic seq + ISO timestamp).
- snapshot(graph, agents): after a run, writes tasks (final state+output), edges, agents.

## Tests (TDD, in-memory DB)

- schema round-trips: create project+run, save tasks/edges/agents/events, read them back equal.
- listRuns ordering (most recent first) + finishRun updates counts.
- config set/get/overwrite/absent.
- appendEvent preserves seq ordering.
- (runtime) RunRecorder: run a graph through the Scheduler with a fake executor + recorder
  observer; assert events + snapshot rows match the run.

## App integration

- CLI: every --graph/--fan run is persisted (auto project "cli"). New commands:
  `--history` lists recent runs; `--show <runId>` prints tasks, edges (A → B), and events.
- GUI: a "History" dock lists past runs from the DB; selecting one loads its tasks + event
  timeline into the views (replay). Each run is saved on completion. DB lives under a
  per-user app data dir.

## Dependency

- SQLiteCpp (bundles sqlite3) via FetchContent. Cross-platform, no system sqlite needed.
