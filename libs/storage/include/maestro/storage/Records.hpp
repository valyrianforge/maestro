#pragma once
#include <cstdint>
#include <string>

namespace maestro::storage {

struct ProjectRecord {
    std::int64_t id{0};
    std::string name;
    std::string rootPath;
    std::string createdAt;
};

struct RunRecord {
    std::int64_t id{0};
    std::int64_t projectId{0};
    std::string topic;
    std::string mode;      // "pipeline", "fan", "single"
    std::string startedAt;
    std::string finishedAt;
    int succeeded{0};
    int failed{0};
    int blocked{0};
};

struct TaskRecord {
    std::int64_t id{0};
    std::int64_t runId{0};
    std::string name;
    std::string provider;
    std::string state;
    int priority{0};
    std::string output;
};

struct EdgeRecord {
    std::int64_t runId{0};
    std::string dependent;    // task that receives the forwarded context
    std::string prerequisite; // task whose output is forwarded
};

struct AgentRecord {
    std::int64_t id{0};
    std::int64_t runId{0};
    std::string name;
    std::string provider;
};

struct EventRecord {
    std::int64_t id{0};
    std::int64_t runId{0};
    int seq{0};            // monotonic order within a run
    std::string ts;        // ISO-8601 timestamp
    std::string type;      // "TaskStarted", "TaskSucceeded", ...
    std::string taskName;
    std::int64_t agentId{0};
    std::string detail;
};

// --- v2 interactive orchestration records ---

// A live ACP session (nucleus or worker). Persisted so the agent graph and its
// nucleus->electron structure survive a restart. Keyed by the opaque session id.
struct SessionRecord {
    std::string sessionId;
    std::string role;      // "nucleus" | "worker" | label
    std::string parentId;  // "" if none (the nucleus); else the parent session id
    std::string state;     // "idle" | "busy" | "done" | "error"
    std::string createdAt;
};

// A fan-out plan the nucleus proposed. workersJson is the opaque serialized
// WorkerSpec list (storage stays decoupled from the acp layer).
struct PlanRecord {
    std::int64_t id{0};
    std::string nucleusSessionId;
    std::string status;      // "proposed" | "approved" | "rejected"
    std::string workersJson;
    std::string createdAt;
};

// One entry in the append-only decision log: approvals, denials, steers, plan
// decisions — the audit trail of who-decided-what.
struct DecisionRecord {
    std::int64_t id{0};
    std::string sessionId; // "" for plan-level decisions
    std::string ts;
    std::string kind;      // "plan_approved" | "tool_allowed" | "tool_denied" | "steer" | ...
    std::string detail;
};

} // namespace maestro::storage
