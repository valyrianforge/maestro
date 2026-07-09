#include "maestro/runtime/ResultCollector.hpp"

namespace maestro::runtime {

void ResultCollector::add(const TaskChunk& chunk) {
    if (chunk.session) {
        session_ = chunk.session;
    }
    switch (chunk.kind) {
    case TaskChunk::Kind::AssistantText:
        assistantText_ += chunk.text;
        break;
    case TaskChunk::Kind::Stdout:
        if (!stdoutText_.empty()) {
            stdoutText_ += '\n';
        }
        stdoutText_ += chunk.text;
        break;
    case TaskChunk::Kind::Stderr:
        stderrText_ += chunk.text;
        break;
    case TaskChunk::Kind::Result:
        sawResult_ = true;
        resultText_ = chunk.text;
        if (chunk.costUsd) {
            costUsd_ = chunk.costUsd;
        }
        if (chunk.isError) {
            error_ = true;
        }
        break;
    case TaskChunk::Kind::Error:
        error_ = true;
        if (!chunk.text.empty()) {
            resultText_ = chunk.text;
        }
        break;
    case TaskChunk::Kind::SessionStarted:
    case TaskChunk::Kind::Other:
        break; // metadata only
    }
}

void ResultCollector::addStderr(std::string_view text) { stderrText_ += text; }

TaskResult ResultCollector::finalize(bool processSucceeded) const {
    TaskResult result;
    result.success = processSucceeded && !error_;
    result.session = session_;
    result.costUsd = costUsd_;

    if (sawResult_ && !resultText_.empty()) {
        result.output = resultText_;
    } else if (!assistantText_.empty()) {
        result.output = assistantText_;
    } else {
        result.output = stdoutText_;
    }
    return result;
}

} // namespace maestro::runtime
