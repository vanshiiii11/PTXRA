#pragma once
#include "Token.h"
#include <string>
#include <vector>

namespace ptxra {

/// Hand-written DFA lexer for PTX assembly.
///
/// The PTX lexer is context-sensitive in one way: dot-prefixed words
/// are keywords (directives / type qualifiers) when they appear in
/// instruction or declaration context.  We handle this by classifying
/// every ".word" we see from a static keyword table; anything not in
/// the table is emitted as Identifier (so future PTX versions still lex).
class Lexer {
public:
    /// Lex an entire source string.
    /// @param source   PTX source text (must outlive this call)
    /// @param filename Used in SourceLocation for diagnostics
    explicit Lexer(std::string source, std::string filename = "<input>");

    /// Produce all tokens (including final Eof).
    std::vector<Token> tokenize();

    /// Returns any diagnostic messages produced (e.g., unknown chars).
    const std::vector<std::string>& diagnostics() const { return diagnostics_; }

private:
    std::string   src_;
    std::string   filename_;
    std::size_t   pos_    = 0;
    uint32_t      line_   = 1;
    uint32_t      col_    = 1;
    std::vector<std::string> diagnostics_;

    // ---- Character stream primitives ----
    char peek(int offset = 0) const;
    char advance();
    bool atEnd() const { return pos_ >= src_.size(); }

    // ---- Skip helpers ----
    void skipLineComment();    // // …
    void skipBlockComment();   // /* … */
    void skipWhitespace();

    // ---- Scanners ----
    Token scanNumber();        // integers and float literals
    Token scanHexFloat();      // 0f…  0d… style PTX hex floats
    Token scanString();        // "…"
    Token scanIdentOrKeyword(); // [a-zA-Z_][a-zA-Z0-9_]*
    Token scanDotWord();       // .[a-zA-Z][a-zA-Z0-9_]*
    Token scanRegister();      // %[a-zA-Z][a-zA-Z0-9_.]* (including %tid.x)
    Token scanPunct();         // single or double-char punctuation

    // ---- Helpers ----
    SourceLocation here() const { return {filename_, line_, col_}; }
    Token makeToken(TokenKind k, std::string text, SourceLocation loc) const;

    /// Classify a dot-word into the appropriate TokenKind.
    static TokenKind classifyDotWord(std::string_view word);
};

} // namespace ptxra
