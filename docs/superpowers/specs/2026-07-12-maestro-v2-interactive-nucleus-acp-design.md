# Maestro v2 — Interactive Nucleus + ACP: Architecture Design

**Status:** Proposed (supersedes the headless portions of the v1 architecture map)
**Date:** 2026-07-12
**Supersedes:** v1 locked decisions #1 (headless one-shot), the "one direction each" seam
semantics, and the "AI tools never talk / no nucleus" principle. Everything else in the v1
map (Qt-free Core, provider/process/storage seams, DAG engine, TDD, SQLite) still holds.

---

## 1. Why v2

v1 chose **headless one-shot** CLI drive (`claude -p`, sessions only *simulated* via `--resume`).
That mode physically cannot support live interaction: no mid-run message injection, no in-memory
approval callback, no live subagent visibility. The app therefore feels dead — you cannot control
a running agent, and there is no "main agent" coordinating others. This is a design-model
mismatch, not a code-quality problem.

v2 replaces the drive model with **live, bidirectional, interactive agent sessions**, adds an
**AI nucleus (supervisor)** that proposes work under human approval, and makes the agent graph a
first-class, steerable surface.

## 2. Locked decisions (v2)

| # | Decision | Choice | Rationale |
|---|----------|--------|-----------|
| 1 | Drive model | **Live ACP sessions.** Maestro is an **Agent Client Protocol** client; each agent is a long-lived subprocess spoken to over JSON-RPC (stdio). | ACP gives streaming, plans, per-tool approval, and cancel natively; one wire format across Claude Code / Codex / Gemini / etc. instead of N bespoke stream-json parsers. |
| 2 | Session control | **Cancel + redirect + queued steer + per-action approval.** | This is the real ceiling of "live steering" — no tool anywhere supports mid-*token* injection. Calibrated to what is physically possible. |
| 3 | Nucleus | **Hybrid supervisor.** The nucleus is an ACP agent whose tools are *orchestration* verbs (propose_plan, spawn_worker, message_worker, read_worker_result). It **proposes**; the human **approves/edits** before anything executes. | Matches the "nucleus and electrons" vision with the human as final authority. |
| 4 | Worker isolation | **Each worker (electron) runs in its own git worktree.** (Recommended; see §7.) | Parallel workers cannot clobber each other; results reviewed as diffs before merge. Standard in the tools that do this well (Conductor, Claude Squad, uzi, Sculptor). |
| 5 | UI seam | **Still "one direction each"** — Core emits events, UI/nucleus send commands — but semantics upgrade to a **live request/response loop** (agent emits `ApprovalRequest`; UI answers `ApprovalDecision`). No shared mutable state. |
| 6 | Kept from v1 | Qt-free Core, `IProcessBackend`/QProcess seam, DAG engine, `ISqlStore`+SQLite, Catch2 TDD, fakeable everything. | The bones are good; reuse them. |

## 3. What ACP gives us (the load-bearing protocol)

ACP (Zed's "LSP for agents", JSON-RPC 2.0 over stdio). Methods we depend on:

- `initialize` / `authenticate` — capability negotiation.
- `session/new` (`cwd`, `mcpServers` → `sessionId`), `session/load`, `session/resume`.
- `session/prompt` (`sessionId`, `prompt: ContentBlock[]`) — resolves at turn end with `stopReason`.
- `session/update` notifications (streamed during a turn), discriminated by `sessionUpdate`:
  `agent_message_chunk`, `agent_thought_chunk`, `plan`, `tool_call`, `tool_call_update`,
  `usage_update`.
- `session/request_permission` (`options[]`: allow_once/allow_always/reject_once/reject_always)
  → client replies `{outcome: selected, optionId}`. **This is the human-in-the-loop gate.**
- `session/cancel` (notification) — interrupt a turn mid-run.
- Client callbacks the agent invokes: `fs/read_text_file`, `fs/write_text_file`, `terminal/*`.

**Integration cost (accepted):** no official C++ SDK, so we hand-write a small ACP/JSON-RPC layer
over `QProcess` + `nlohmann/json` from the published JSON Schema; and Claude Code / Codex need
their Node ACP adapters (`claude-code-acp`, `codex-acp`) bundled until native. Still far cheaper
than maintaining a zoo of stream-json parsers.

## 4. Architecture (deltas from v1)

```
┌───────────────────────────────────────────────────────────────────────┐
│ UI LAYER (Qt6)                                                          │
│  Live Agent Graph (nucleus→electrons) · per-node Chat/Inspector ·       │
│  Plan-approval cards · Action-approval prompts · Logs · Resource mon.    │
│   commands ↓ (prompt / approve / deny / cancel / steer)   events ↑       │
└───────────────▲───────────────────────────────────────┬────────────────┘
                │                                        │
┌───────────────┴────────────────────────────────────────▼───────────────┐
│ CORE ENGINE (no Qt)                                                      │
│  Orchestrator · Nucleus(supervisor loop) · SessionManager ·             │
│  ApprovalBroker · SteerQueue · TaskGraph(DAG) · WorkspaceManager ·       │
│  EventBus · CommandBus                                                   │
└──┬───────────────┬──────────────────┬──────────────────┬────────────────┘
   │               │                  │                  │
┌──▼──────────┐ ┌──▼───────────┐ ┌────▼───────┐   ┌──────▼──────┐
│ ACP CLIENT  │ │ WORKTREE MGR │ │ STORAGE    │   │  PROCESS    │
│ (JSON-RPC/  │ │ (git worktree│ │ (SQLite)   │   │  backend    │
│  stdio)     │ │  per worker) │ │            │   │ (QProcess)  │
└──┬──────────┘ └──────────────┘ └────────────┘   └──────┬──────┘
   │  spawns + speaks ACP to ↓                            │ spawns
   └──────────► agent subprocesses (claude-code-acp, codex-acp, …) ◄──────┘
```

### New Core components

- **`AcpClient`** — owns one agent subprocess; frames/parses JSON-RPC; exposes typed async calls
  (`newSession`, `prompt`, `cancel`) and emits typed events (message chunk, plan, tool_call,
  permission request). Sits behind an `IAgentSession` interface so it is fakeable (no real
  subprocess in unit tests).
- **`SessionManager`** — lifecycle of all live sessions (nucleus + workers); maps sessionId ↔
  Agent node; restart/cleanup.
- **`Nucleus`** — the supervisor loop: feeds the goal to the nucleus ACP session, receives its
  `propose_plan` tool call, routes it to the `ApprovalBroker`, and on approval spawns workers.
- **`ApprovalBroker`** — turns `session/request_permission` (and plan proposals) into UI events,
  awaits an `ApprovalDecision` command, replies to the agent. Supports auto-rules (e.g.
  allow_always for read-only tools) so not everything blocks on a human.
- **`SteerQueue`** — per-worker queue of human/nucleus messages delivered at the worker's next
  turn boundary (non-interrupting steering); plus `cancel` for hard redirects.

## 5. Bidirectional data flow (one unit of work)

```
You → goal → Nucleus session
  → nucleus emits propose_plan(workers[])         (streamed as a `plan` + tool_call)
  → ApprovalBroker → UI plan card → 🧑 approve/edit/reject
  → on approval: SessionManager spawns N worker ACP sessions, each in its own worktree
  → each worker streams message/thought/tool_call; risky tool_call → request_permission
      → ApprovalBroker → 🧑 allow/deny (or auto-rule)
  → worker results (diffs/artifacts) → Workspace → up the "string" to the Nucleus
  → Nucleus proposes next move; you may cancel/redirect/steer any node at any time
  → every state delta → EventBus → Graph animates (node state, edge message particles)
```

## 6. The live graph (built on existing `GraphCanvas`)

Reuse the milestone-6 `QGraphicsScene` canvas. Additions:
- Node states: pending(grey) → running(orange + pulse) → success(green) → error(red) → stale.
- Running-node pulse via `QPropertyAnimation`; glow via `QGraphicsDropShadowEffect`.
- **Message particles**: a `QGraphicsEllipseItem` animated along the existing edge
  `QGraphicsPathItem` (via `QPainterPath::pointAtPercent`) when a message crosses a string.
- Click node → `QDockWidget` inspector: that agent's messages, plus approve/deny/cancel/steer.
- Hierarchical layout rooted at the nucleus (extends existing depth-based `relayout`), animate
  `pos()` on spawn so the graph doesn't jump.

## 7. Open decision → recommendation: worker isolation

**Recommendation: each electron gets its own git worktree** (`WorktreeManager`), branched off the
project. Workers run fully parallel with zero file collisions; the nucleus/human reviews each
worker's diff before it merges to the integration branch. Docker is a later hardening option.
*Alternative (rejected for v1):* shared workspace — simpler but workers stomp each other, which is
exactly the failure mode parallel agents are prone to.

## 8. Milestone plan (each = spec-light → TDD → PR into `staging`)

1. **ACP client core** — `AcpClient` + JSON-RPC framing over the existing process backend;
   `initialize`/`session/new`/`prompt`/`session/update` against a **checked-in fake ACP agent**.
   Pure Core, no Qt, fully TDD. *(Proves the load-bearing risk, like v1's process spine.)*
2. **Approval + cancel** — `ApprovalBroker`, `request_permission` round-trip, `session/cancel`,
   auto-rules. Fake-agent-driven tests.
3. **Session manager + steer queue** — many live sessions, non-interrupting steer delivery.
4. **Nucleus loop** — orchestration tools (propose_plan/spawn_worker/...), plan approval gate.
5. **Worktree isolation** — `WorktreeManager`, per-worker branch, diff capture.
6. **Persistence** — extend `ISqlStore` for live sessions, plans, approvals, decision log.
7. **UI: live graph + inspector** — states, pulses, message particles, click-to-steer.
8. **UI: approval surfaces** — plan cards, action prompts, auto-rule settings.
9. **Real adapters** — bundle `claude-code-acp` / `codex-acp`; end-to-end against real Claude Code.
10. **Polish** — resource monitor, broadcast-steer to a subset of workers, hardening.

## 9. Non-goals (v2)

- Mid-token interruption (physically unsupported anywhere).
- Distributed / multi-machine orchestration.
- Remote ACP transport (stdio only for v1 of v2).
- Non-git isolation (Docker) — deferred.

## 10. Testing

- Unit: ACP framing/parsing, approval routing, steer-queue ordering, nucleus plan handling — all
  against a **fake ACP agent** (a checked-in script emitting canned JSON-RPC). No real subprocess,
  no network, in-memory SQLite.
- Integration: real `QProcess` driving the fake ACP agent (proves spawn/frame/stream/cancel).
- Contract: any `IAgentSession` impl runs the same shared suite.
