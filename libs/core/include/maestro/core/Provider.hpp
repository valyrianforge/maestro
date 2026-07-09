#pragma once
#include <optional>
#include <string>
#include <string_view>

#include "maestro/core/ProcessSpec.hpp"
#include "maestro/core/SessionId.hpp"

namespace maestro::core {

// Human/stable identifier for a provider implementation, e.g. "claude".
struct ProviderId {
    std::string name;
    friend bool operator==(const ProviderId&, const ProviderId&) = default;
};

// What a provider's underlying tool can do. The scheduler consults these
// instead of special-casing concrete providers.
struct Capabilities {
    bool streaming{false};      // emits incremental output while running
    bool sessionResume{false};  // can resume a prior session by id
    bool reportsCost{false};    // reports token/USD cost on completion
};

// A unit of work handed to a provider to turn into a process launch.
struct TaskRequest {
    std::string prompt;
    std::optional<SessionId> resume;            // resume this session if set
    std::optional<std::string> workingDirectory;
};

// One structured event parsed from a provider's output stream. Deliberately
// free of any JSON type so the core stays dependency-light; providers parse
// their own formats and populate these typed fields.
struct TaskChunk {
    enum class Kind {
        SessionStarted, // session id became known (e.g. init event)
        AssistantText,  // a piece of the model's answer
        Stdout,         // raw stdout passthrough (non-structured tools)
        Stderr,         // raw stderr passthrough
        Result,         // terminal result (final text + metadata)
        Error,          // provider/tool reported an error
        Other,          // recognized-but-uninteresting frame (ignored by most)
    };

    Kind kind{Kind::Other};
    std::string text;                    // assistant delta, result text, or raw line
    std::optional<SessionId> session;    // set when the frame reveals a session id
    std::optional<double> costUsd;       // set on Result when the tool reports cost
    bool isError{false};                 // set on Result/Error frames
};

// The abstraction the scheduler talks to. Adding a new tool means adding one
// IProvider implementation (or a plugin) — no scheduler changes. Two
// responsibilities:
//   1. buildSpec: translate a TaskRequest into a concrete ProcessSpec.
//   2. parseFrame: turn ONE complete output frame (an NDJSON line, a text line)
//      into a structured TaskChunk. Byte-buffering/line-splitting is the
//      caller's job (see NdjsonLineReader) so this stays pure and per-frame.
class IProvider {
public:
    virtual ~IProvider() = default;

    [[nodiscard]] virtual ProviderId id() const = 0;
    [[nodiscard]] virtual Capabilities capabilities() const = 0;
    [[nodiscard]] virtual ProcessSpec buildSpec(const TaskRequest& request) const = 0;

    // Returns nullopt for frames the provider chooses to ignore (e.g. blank
    // lines or noise). Must never throw on malformed input.
    [[nodiscard]] virtual std::optional<TaskChunk> parseFrame(std::string_view frame) const = 0;
};

} // namespace maestro::core
