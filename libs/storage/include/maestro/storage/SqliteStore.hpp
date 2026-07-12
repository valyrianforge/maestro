#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "maestro/storage/Records.hpp"

namespace SQLite {
class Database;
}

namespace maestro::storage {

// Repository over a SQLite database. Creates its schema on open (idempotent).
// Qt-free and orchestrator-free: pure persistence, testable against ":memory:".
// A convenience isoNow() is provided for callers that need timestamps.
class SqliteStore {
public:
    explicit SqliteStore(const std::string& path);
    ~SqliteStore();

    SqliteStore(const SqliteStore&) = delete;
    SqliteStore& operator=(const SqliteStore&) = delete;

    // --- writes ---
    std::int64_t createProject(const std::string& name, const std::string& rootPath,
                               const std::string& createdAt);
    std::int64_t createRun(std::int64_t projectId, const std::string& topic,
                           const std::string& mode, const std::string& startedAt);
    void saveTask(std::int64_t runId, const TaskRecord& task);
    void saveEdge(std::int64_t runId, const std::string& dependent,
                  const std::string& prerequisite);
    std::int64_t saveAgent(std::int64_t runId, const std::string& name,
                           const std::string& provider);
    void appendEvent(std::int64_t runId, const EventRecord& event);
    void finishRun(std::int64_t runId, const std::string& finishedAt, int succeeded, int failed,
                   int blocked);

    // --- reads ---
    [[nodiscard]] std::vector<RunRecord> listRuns(int limit = 50);
    [[nodiscard]] std::optional<RunRecord> getRun(std::int64_t runId);
    [[nodiscard]] std::vector<TaskRecord> tasksForRun(std::int64_t runId);
    [[nodiscard]] std::vector<EdgeRecord> edgesForRun(std::int64_t runId);
    [[nodiscard]] std::vector<AgentRecord> agentsForRun(std::int64_t runId);
    [[nodiscard]] std::vector<EventRecord> eventsForRun(std::int64_t runId);

    // --- v2: live sessions (nucleus + workers), survive restart ---
    void upsertSession(const SessionRecord& session);
    void updateSessionState(const std::string& sessionId, const std::string& state);
    void removeSession(const std::string& sessionId);
    [[nodiscard]] std::vector<SessionRecord> listSessions();
    [[nodiscard]] std::vector<SessionRecord> childSessions(const std::string& parentId);

    // --- v2: proposed plans + approval status ---
    std::int64_t savePlan(const PlanRecord& plan);
    void setPlanStatus(std::int64_t planId, const std::string& status);
    [[nodiscard]] std::optional<PlanRecord> getPlan(std::int64_t planId);

    // --- v2: append-only decision log ---
    std::int64_t appendDecision(const DecisionRecord& decision);
    [[nodiscard]] std::vector<DecisionRecord> decisionsForSession(const std::string& sessionId);
    [[nodiscard]] std::vector<DecisionRecord> recentDecisions(int limit = 100);

    // --- config key/value ---
    void setConfig(const std::string& key, const std::string& value);
    [[nodiscard]] std::optional<std::string> getConfig(const std::string& key);

    // Current UTC time as an ISO-8601 string (convenience for callers).
    [[nodiscard]] static std::string isoNow();

private:
    void migrate();

    std::unique_ptr<SQLite::Database> db_;
};

} // namespace maestro::storage
