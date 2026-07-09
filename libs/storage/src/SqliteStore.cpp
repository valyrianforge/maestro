#include "maestro/storage/SqliteStore.hpp"

#include <SQLiteCpp/SQLiteCpp.h>

#include <chrono>
#include <ctime>

namespace maestro::storage {

SqliteStore::SqliteStore(const std::string& path)
    : db_(std::make_unique<SQLite::Database>(
          path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    db_->exec("PRAGMA foreign_keys = ON;");
    migrate();
}

SqliteStore::~SqliteStore() = default;

void SqliteStore::migrate() {
    db_->exec(R"(
        CREATE TABLE IF NOT EXISTS projects (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            root_path TEXT NOT NULL,
            created_at TEXT NOT NULL);
        CREATE TABLE IF NOT EXISTS runs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            project_id INTEGER NOT NULL,
            topic TEXT NOT NULL,
            mode TEXT NOT NULL,
            started_at TEXT NOT NULL,
            finished_at TEXT,
            succeeded INTEGER DEFAULT 0,
            failed INTEGER DEFAULT 0,
            blocked INTEGER DEFAULT 0);
        CREATE TABLE IF NOT EXISTS tasks (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            provider TEXT NOT NULL,
            state TEXT NOT NULL,
            priority INTEGER DEFAULT 0,
            output TEXT);
        CREATE TABLE IF NOT EXISTS task_edges (
            run_id INTEGER NOT NULL,
            dependent TEXT NOT NULL,
            prerequisite TEXT NOT NULL);
        CREATE TABLE IF NOT EXISTS agents (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER NOT NULL,
            name TEXT NOT NULL,
            provider TEXT NOT NULL);
        CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER NOT NULL,
            seq INTEGER NOT NULL,
            ts TEXT NOT NULL,
            type TEXT NOT NULL,
            task_name TEXT,
            agent_id INTEGER,
            detail TEXT);
        CREATE TABLE IF NOT EXISTS config (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL);
        CREATE INDEX IF NOT EXISTS idx_runs_project ON runs(project_id);
        CREATE INDEX IF NOT EXISTS idx_tasks_run ON tasks(run_id);
        CREATE INDEX IF NOT EXISTS idx_events_run ON events(run_id, seq);
    )");
}

std::int64_t SqliteStore::createProject(const std::string& name, const std::string& rootPath,
                                        const std::string& createdAt) {
    SQLite::Statement q(*db_,
                        "INSERT INTO projects(name, root_path, created_at) VALUES(?,?,?)");
    q.bind(1, name);
    q.bind(2, rootPath);
    q.bind(3, createdAt);
    q.exec();
    return db_->getLastInsertRowid();
}

std::int64_t SqliteStore::createRun(std::int64_t projectId, const std::string& topic,
                                    const std::string& mode, const std::string& startedAt) {
    SQLite::Statement q(
        *db_, "INSERT INTO runs(project_id, topic, mode, started_at) VALUES(?,?,?,?)");
    q.bind(1, projectId);
    q.bind(2, topic);
    q.bind(3, mode);
    q.bind(4, startedAt);
    q.exec();
    return db_->getLastInsertRowid();
}

void SqliteStore::saveTask(std::int64_t runId, const TaskRecord& task) {
    SQLite::Statement q(*db_,
                        "INSERT INTO tasks(run_id, name, provider, state, priority, output) "
                        "VALUES(?,?,?,?,?,?)");
    q.bind(1, runId);
    q.bind(2, task.name);
    q.bind(3, task.provider);
    q.bind(4, task.state);
    q.bind(5, task.priority);
    q.bind(6, task.output);
    q.exec();
}

void SqliteStore::saveEdge(std::int64_t runId, const std::string& dependent,
                           const std::string& prerequisite) {
    SQLite::Statement q(
        *db_, "INSERT INTO task_edges(run_id, dependent, prerequisite) VALUES(?,?,?)");
    q.bind(1, runId);
    q.bind(2, dependent);
    q.bind(3, prerequisite);
    q.exec();
}

std::int64_t SqliteStore::saveAgent(std::int64_t runId, const std::string& name,
                                    const std::string& provider) {
    SQLite::Statement q(*db_, "INSERT INTO agents(run_id, name, provider) VALUES(?,?,?)");
    q.bind(1, runId);
    q.bind(2, name);
    q.bind(3, provider);
    q.exec();
    return db_->getLastInsertRowid();
}

void SqliteStore::appendEvent(std::int64_t runId, const EventRecord& event) {
    SQLite::Statement q(*db_,
                        "INSERT INTO events(run_id, seq, ts, type, task_name, agent_id, detail) "
                        "VALUES(?,?,?,?,?,?,?)");
    q.bind(1, runId);
    q.bind(2, event.seq);
    q.bind(3, event.ts);
    q.bind(4, event.type);
    q.bind(5, event.taskName);
    q.bind(6, event.agentId);
    q.bind(7, event.detail);
    q.exec();
}

void SqliteStore::finishRun(std::int64_t runId, const std::string& finishedAt, int succeeded,
                            int failed, int blocked) {
    SQLite::Statement q(*db_,
                        "UPDATE runs SET finished_at=?, succeeded=?, failed=?, blocked=? "
                        "WHERE id=?");
    q.bind(1, finishedAt);
    q.bind(2, succeeded);
    q.bind(3, failed);
    q.bind(4, blocked);
    q.bind(5, runId);
    q.exec();
}

std::vector<RunRecord> SqliteStore::listRuns(int limit) {
    std::vector<RunRecord> out;
    SQLite::Statement q(*db_,
                        "SELECT id, project_id, topic, mode, started_at, "
                        "IFNULL(finished_at,''), succeeded, failed, blocked "
                        "FROM runs ORDER BY id DESC LIMIT ?");
    q.bind(1, limit);
    while (q.executeStep()) {
        RunRecord r;
        r.id = q.getColumn(0).getInt64();
        r.projectId = q.getColumn(1).getInt64();
        r.topic = q.getColumn(2).getString();
        r.mode = q.getColumn(3).getString();
        r.startedAt = q.getColumn(4).getString();
        r.finishedAt = q.getColumn(5).getString();
        r.succeeded = q.getColumn(6).getInt();
        r.failed = q.getColumn(7).getInt();
        r.blocked = q.getColumn(8).getInt();
        out.push_back(std::move(r));
    }
    return out;
}

std::optional<RunRecord> SqliteStore::getRun(std::int64_t runId) {
    SQLite::Statement q(*db_,
                        "SELECT id, project_id, topic, mode, started_at, "
                        "IFNULL(finished_at,''), succeeded, failed, blocked FROM runs WHERE id=?");
    q.bind(1, runId);
    if (!q.executeStep()) {
        return std::nullopt;
    }
    RunRecord r;
    r.id = q.getColumn(0).getInt64();
    r.projectId = q.getColumn(1).getInt64();
    r.topic = q.getColumn(2).getString();
    r.mode = q.getColumn(3).getString();
    r.startedAt = q.getColumn(4).getString();
    r.finishedAt = q.getColumn(5).getString();
    r.succeeded = q.getColumn(6).getInt();
    r.failed = q.getColumn(7).getInt();
    r.blocked = q.getColumn(8).getInt();
    return r;
}

std::vector<TaskRecord> SqliteStore::tasksForRun(std::int64_t runId) {
    std::vector<TaskRecord> out;
    SQLite::Statement q(*db_,
                        "SELECT id, name, provider, state, priority, IFNULL(output,'') "
                        "FROM tasks WHERE run_id=? ORDER BY id");
    q.bind(1, runId);
    while (q.executeStep()) {
        TaskRecord t;
        t.id = q.getColumn(0).getInt64();
        t.runId = runId;
        t.name = q.getColumn(1).getString();
        t.provider = q.getColumn(2).getString();
        t.state = q.getColumn(3).getString();
        t.priority = q.getColumn(4).getInt();
        t.output = q.getColumn(5).getString();
        out.push_back(std::move(t));
    }
    return out;
}

std::vector<EdgeRecord> SqliteStore::edgesForRun(std::int64_t runId) {
    std::vector<EdgeRecord> out;
    SQLite::Statement q(*db_,
                        "SELECT dependent, prerequisite FROM task_edges WHERE run_id=?");
    q.bind(1, runId);
    while (q.executeStep()) {
        EdgeRecord e;
        e.runId = runId;
        e.dependent = q.getColumn(0).getString();
        e.prerequisite = q.getColumn(1).getString();
        out.push_back(std::move(e));
    }
    return out;
}

std::vector<AgentRecord> SqliteStore::agentsForRun(std::int64_t runId) {
    std::vector<AgentRecord> out;
    SQLite::Statement q(*db_, "SELECT id, name, provider FROM agents WHERE run_id=? ORDER BY id");
    q.bind(1, runId);
    while (q.executeStep()) {
        AgentRecord a;
        a.id = q.getColumn(0).getInt64();
        a.runId = runId;
        a.name = q.getColumn(1).getString();
        a.provider = q.getColumn(2).getString();
        out.push_back(std::move(a));
    }
    return out;
}

std::vector<EventRecord> SqliteStore::eventsForRun(std::int64_t runId) {
    std::vector<EventRecord> out;
    SQLite::Statement q(*db_,
                        "SELECT id, seq, ts, type, IFNULL(task_name,''), IFNULL(agent_id,0), "
                        "IFNULL(detail,'') FROM events WHERE run_id=? ORDER BY seq");
    q.bind(1, runId);
    while (q.executeStep()) {
        EventRecord e;
        e.id = q.getColumn(0).getInt64();
        e.runId = runId;
        e.seq = q.getColumn(1).getInt();
        e.ts = q.getColumn(2).getString();
        e.type = q.getColumn(3).getString();
        e.taskName = q.getColumn(4).getString();
        e.agentId = q.getColumn(5).getInt64();
        e.detail = q.getColumn(6).getString();
        out.push_back(std::move(e));
    }
    return out;
}

void SqliteStore::setConfig(const std::string& key, const std::string& value) {
    SQLite::Statement q(*db_,
                        "INSERT INTO config(key,value) VALUES(?,?) "
                        "ON CONFLICT(key) DO UPDATE SET value=excluded.value");
    q.bind(1, key);
    q.bind(2, value);
    q.exec();
}

std::optional<std::string> SqliteStore::getConfig(const std::string& key) {
    SQLite::Statement q(*db_, "SELECT value FROM config WHERE key=?");
    q.bind(1, key);
    if (!q.executeStep()) {
        return std::nullopt;
    }
    return q.getColumn(0).getString();
}

std::string SqliteStore::isoNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

} // namespace maestro::storage
