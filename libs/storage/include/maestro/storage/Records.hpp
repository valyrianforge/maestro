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

} // namespace maestro::storage
