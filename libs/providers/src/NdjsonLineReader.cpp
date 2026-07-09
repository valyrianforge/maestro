#include "maestro/providers/NdjsonLineReader.hpp"

namespace maestro::providers {

std::vector<std::string> NdjsonLineReader::feed(std::string_view bytes) {
    std::vector<std::string> lines;
    buffer_.append(bytes);

    std::size_t start = 0;
    for (std::size_t i = 0; i < buffer_.size(); ++i) {
        if (buffer_[i] != '\n') {
            continue;
        }
        std::size_t end = i;
        if (end > start && buffer_[end - 1] == '\r') {
            --end; // strip CR for CRLF streams
        }
        lines.emplace_back(buffer_.substr(start, end - start));
        start = i + 1;
    }
    buffer_.erase(0, start);
    return lines;
}

std::optional<std::string> NdjsonLineReader::flush() {
    if (buffer_.empty()) {
        return std::nullopt;
    }
    std::string remaining = std::move(buffer_);
    buffer_.clear();
    if (!remaining.empty() && remaining.back() == '\r') {
        remaining.pop_back();
    }
    if (remaining.empty()) {
        return std::nullopt;
    }
    return remaining;
}

} // namespace maestro::providers
