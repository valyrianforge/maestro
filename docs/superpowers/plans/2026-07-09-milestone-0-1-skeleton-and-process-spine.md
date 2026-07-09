# Milestone 0–1: Skeleton & Process Spine — Implementation Plan

> **For agentic workers:** Steps use checkbox (`- [ ]`) syntax. TDD, frequent commits.

**Goal:** Stand up the Maestro build skeleton and the load-bearing process-management
spine (spawn/stream/detect-exit/restart) behind a fakeable interface — fully unit-tested
without any real CLI or Qt.

**Architecture:** A Qt-free `maestro_core` static lib holds pure interfaces + domain types.
`maestro_process` holds the process abstraction: an `IProcessBackend` seam with a
`FakeProcessBackend` (tests) and an optional `QtProcessBackend` (guarded by
`MAESTRO_WITH_QT`, off by default until Qt is installed). `ProcessManager` orchestrates
spawn/stream/restart against the interface. Catch2 via FetchContent.

**Tech Stack:** C++20, CMake ≥3.24, Ninja, Catch2 v3, clang 17.

## Global Constraints

- Qt-free Core: `maestro_core` and `maestro_process` (non-Qt parts) never `#include` Qt.
- C++20 standard, warnings-as-errors on our targets (`-Wall -Wextra -Werror`).
- Every external dependency crosses a fakeable interface.
- Cross-platform intent: no POSIX-only APIs in portable code; Qt backend isolated.
- Frequent commits; each task ends green.

---

## File Structure

```
CMakeLists.txt                        # top-level: options, C++20, Catch2, subdirs
cmake/Warnings.cmake                  # warning flags helper
libs/core/CMakeLists.txt
libs/core/include/maestro/core/Ids.hpp          # strong id types
libs/core/include/maestro/core/ProcessSpec.hpp  # ProcessSpec, ProcessEvent
libs/process/CMakeLists.txt
libs/process/include/maestro/process/IProcessBackend.hpp
libs/process/include/maestro/process/ProcessManager.hpp
libs/process/src/ProcessManager.cpp
libs/process/include/maestro/process/FakeProcessBackend.hpp
libs/process/src/FakeProcessBackend.cpp
tests/CMakeLists.txt
tests/unit/test_process_manager.cpp
```

---

### Task 0: Build skeleton + first passing test

**Files:** top-level `CMakeLists.txt`, `cmake/Warnings.cmake`, `tests/CMakeLists.txt`,
`tests/unit/test_smoke.cpp`, `libs/core` headers.

- [ ] Top-level CMake: project, C++20, `MAESTRO_WITH_QT` option (default OFF), FetchContent Catch2, `enable_testing()`, add subdirs.
- [ ] Core lib: header-only interface target `maestro_core` exposing `Ids.hpp`, `ProcessSpec.hpp`.
- [ ] Smoke test asserting a `ProcessSpec` can be constructed.
- [ ] Configure + build + `ctest` green. Commit.

### Task 1: ProcessManager spawn + stream (against FakeProcessBackend)

**Interfaces produced:**
- `IProcessBackend`: `start(int handle, const ProcessSpec&)`, `write(int,std::string_view)`,
  `kill(int)`; callbacks `onStdout/onStderr/onExit` set by the manager.
- `ProcessManager`: `Handle spawn(ProcessSpec, Callbacks)`, `void writeStdin(Handle,string)`,
  streams stdout/stderr to callbacks, reports exit.

- [ ] Test: spawning routes backend stdout → manager `onOutput` callback.
- [ ] Implement `IProcessBackend`, `FakeProcessBackend` (scriptable emissions), `ProcessManager`.
- [ ] Green. Commit.

### Task 2: Exit detection + restart policy

- [ ] Test: a process that exits non-zero with `RestartPolicy{maxRestarts:2}` is restarted twice then reported failed.
- [ ] Test: exit code 0 is reported as success, no restart.
- [ ] Implement restart accounting in `ProcessManager`.
- [ ] Green. Commit.

### Task 3: Graceful kill + write-after-exit safety

- [ ] Test: `kill(handle)` triggers `onExit` with a killed status; subsequent `writeStdin` is a no-op (no crash).
- [ ] Implement kill + guard writes to dead handles.
- [ ] Green. Commit.

### Task 4 (deferred until Qt present): QtProcessBackend

- [ ] Behind `MAESTRO_WITH_QT`; wraps `QProcess`. Compiled only when Qt6 found. Not required for M0–1 green.

---

## Self-review notes
- Spec coverage: M0 (skeleton, Catch2, tri-platform intent) + M1 (spawn/stream/exit/restart,
  fakeable seam) covered by Tasks 0–3. QtProcessBackend real build deferred (Qt not installed) — Task 4.
- No placeholders: all tasks carry concrete tests below in implementation.
