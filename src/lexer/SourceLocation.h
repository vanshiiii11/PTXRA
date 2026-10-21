#pragma once
#include <cstdint>
#include <string>

namespace ptxra {

/// Identifies a location in a source file for diagnostics.
struct SourceLocation {
    std::string filename;
    uint32_t    line   = 0; // 1-based
    uint32_t    column = 0; // 1-based

    SourceLocation() = default;
    SourceLocation(std::string f, uint32_t l, uint32_t c)
        : filename(std::move(f)), line(l), column(c) {}

    std::string toString() const {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }

    bool isValid() const { return line > 0; }
};

} // namespace ptxra
