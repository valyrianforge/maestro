# Milestone 2: Provider Layer — Implementation Plan

**Goal:** Introduce the provider abstraction (`IProvider`) plus a real `ClaudeProvider`
and a `GenericCliProvider`, so the scheduler can drive any CLI through one interface —
building the launch spec and parsing the tool's streamed output into structured
`TaskChunk`s.

**Architecture:** `IProvider` + the shared value types (`TaskRequest`, `TaskChunk`,
`Capabilities`, `ProviderId`, `SessionId`) live in the Qt-free, JSON-free core. Concrete
providers live in `libs/providers`, which depends on nlohmann/json. NDJSON framing is a
separate reusable `NdjsonLineReader` so `IProvider::parseFrame` stays pure and per-line.

## Verified facts (captured from the real `claude` CLI, 2026-07-09)

- `claude -p <prompt> --output-format stream-json` **requires `--verbose`**.
- Output is **NDJSON**; each line is a JSON object keyed by `type`:
  - `system`/`init` → carries `session_id`, `model`, `tools`.
  - `system`/`hook_*` → hook noise; ignored for content.
  - `assistant` → `message.content[]` array of `{type:"text", text}` blocks = the answer.
  - `rate_limit_event` → ignored for content.
  - `result` → final: `result` (full text), `total_cost_usd`, `is_error`, `duration_ms`.
- `session_id` is a **UUID string**, present on most lines; authoritative on `init`/`result`.

## Design decisions

- `SessionId` becomes a **string** wrapper (was `Id<uint64>`); the old alias is removed.
- `TaskChunk` carries **typed fields** (text, kind, optional session/cost/exitCode/isError),
  NOT a raw json blob — this keeps core JSON-free; providers do all parsing internally.
- `IProvider::parseFrame(frame) -> optional<TaskChunk>`: one complete NDJSON line in, one
  chunk out (nullopt to ignore). Byte-buffering is `NdjsonLineReader`'s job, not the provider's.
- Dropped the spec's separate `extractSession()` — session lives on the chunk (YAGNI).

## Tasks (TDD)

- **T1 SessionId + Provider types** (core): `SessionId.hpp`, `Provider.hpp`. Remove old
  `SessionTag`/`SessionId` from `Ids.hpp`. Test: type construction + validity.
- **T2 NdjsonLineReader**: buffer bytes → complete lines; `flush()` on EOF. Tests: split
  across chunk boundaries, multiple lines per feed, trailing partial line.
- **T3 ClaudeProvider::buildSpec**: emits `-p <prompt> --output-format stream-json --verbose`,
  adds `--resume <id>` when `resume` set, sets cwd. Tests: arg vector exact match.
- **T4 ClaudeProvider::parseFrame**: real fixture lines → assistant text, result (cost/text),
  init (session), ignore hooks/rate-limit. Tests using captured fixtures.
- **T5 GenericCliProvider**: `{{prompt}}` substitution in an arg template; parseFrame passes
  lines through as Stdout. Tests.
- **T6 Provider contract test**: a shared suite every IProvider must satisfy (id non-empty,
  buildSpec includes program, parseFrame tolerates junk without throwing).
- **T7 maestro-cli integration**: claude path uses ClaudeProvider + NdjsonLineReader to render
  assistant text live and a final cost/duration summary. `--exec` stays raw passthrough.

## Dependencies added

- nlohmann/json v3.11.3 via FetchContent, linked only by `maestro_providers`.
