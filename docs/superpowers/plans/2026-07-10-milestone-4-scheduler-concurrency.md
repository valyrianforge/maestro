# Milestone 4: Scheduler + Concurrency — Implementation Plan

**Goal:** Replace the sequential dispatch loop with a concurrent `Scheduler` that runs
independent ready tasks in parallel up to a bounded worker count, with priority ordering,
per-task retry, and pause/resume — the real "hundreds of agents" capability.

**Architecture:** A `Scheduler` owns the execution loop. Worker threads (bounded by
`maxConcurrency`) each run one `ITaskExecutor::execute` at a time; results flow back to the
scheduler thread through a mutex+condvar completion queue. The **task graph is mutated only
by the scheduler thread** (workers never touch it), so no graph locking is needed. This is
safe because `ProcessTaskExecutor::execute` is already self-contained (fresh backend per
call via the factory), so concurrent execution has no shared mutable state.

`Orchestrator` becomes a thin facade over `Scheduler` (default `maxConcurrency=1` preserves
M3's deterministic behavior and all existing tests). GUI/CLI can opt into higher concurrency.

## Design

- `SchedulerConfig { int maxConcurrency = 1; int maxRetries = 0; }`.
- Completion queue: `{TaskId, AgentId, TaskResult}` guarded by mutex+condvar.
- Loop (scheduler thread, holding the lock except while waiting):
  1. If not paused, dispatch highest-priority ready tasks while `inFlight < maxConcurrency`:
     mark Running, lease agent, compose forwarded prompt, spawn a worker running `execute`.
  2. If nothing in flight: if paused, wait for resume; else if no ready tasks, done.
  3. Otherwise wait for a completion; drain all completions:
     - success → markSucceeded + store artifact + event;
     - failure with retries left → requeue (state back to Pending), increment retry count;
     - failure exhausted → markFailed (propagates Blocked) + event.
  4. Join all workers at the end.
- `pause()`/`resume()` toggle an atomic flag and notify the condvar.
- Shared types `OrchestratorEvent` + `RunReport` move into `Scheduler.hpp`; `Orchestrator.hpp`
  includes it (breaks the would-be circular include).

## Tests (thread-safe fake executor that records max observed concurrency)

- concurrency: N independent tasks with maxConcurrency=K observe peak concurrency == min(N,K).
- dependencies respected under concurrency (a dependent never starts before its prereq ends).
- retry: fail twice then succeed with maxRetries>=2 → succeeded, executed 3x.
- retry exhausted → failed, dependents blocked.
- pause() halts new dispatch; resume() completes the run.
- priority ordering with a single slot.
- maxConcurrency=1 reproduces the exact M3 sequential order (regression).

## CMake

- Add `Scheduler.cpp` to maestro_orchestrator; `find_package(Threads)`, link `Threads::Threads`.

## Demo

`maestro-cli --fan <N> "<prompt>"`: N independent Claude tasks run concurrently (bounded),
demonstrating wall-clock speedup vs sequential.
