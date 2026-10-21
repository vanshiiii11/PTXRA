#pragma once
#include "../lexer/SourceLocation.h"
#include <stdexcept>
#include <string>

namespace ptxra {

/// Thrown by Parser on unrecoverable syntax errors.
class ParseError : public std::runtime_error {
public:
    SourceLocation loc;

    ParseError(const std::string& msg, SourceLocation l)
        : std::runtime_error(l.toString() + ": " + msg), loc(std::move(l)) {}
};

/// Thrown on unsupported-but-lexable constructs (unknown opcode, etc.)
class UnsupportedFeature : public std::runtime_error {
public:
    SourceLocation loc;

    UnsupportedFeature(const std::string& msg, SourceLocation l)
        : std::runtime_error(l.toString() + ": unsupported: " + msg), loc(std::move(l)) {}
};

} // namespace ptxra
