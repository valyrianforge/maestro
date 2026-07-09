#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace maestro::providers {

// Reassembles a byte stream (delivered in arbitrary chunks) into complete,
// newline-delimited lines. Process output does not arrive on line boundaries,
// so this buffers partial lines across feed() calls. Returned lines exclude the
// trailing '\n' (and a trailing '\r' if present, for CRLF tolerance).
class NdjsonLineReader {
public:
    // Append bytes; return every complete line they completed. A partial final
    // line is retained internally for the next feed().
    std::vector<std::string> feed(std::string_view bytes);

    // On EOF, return any buffered partial line (non-empty), else nullopt.
    std::optional<std::string> flush();

private:
    std::string buffer_;
};

} // namespace maestro::providers
