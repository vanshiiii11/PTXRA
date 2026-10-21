#include "Lexer.h"
#include "../support/StringUtil.h"
#include <cassert>
#include <cctype>
#include <unordered_map>

namespace ptxra {

// ============================================================
// Static keyword table for dot-words
// ============================================================

static const std::unordered_map<std::string, TokenKind>& dotKeywords() {
    static const std::unordered_map<std::string, TokenKind> table = {
        {".version",      TokenKind::DotVersion},
        {".target",       TokenKind::DotTarget},
        {".address_size", TokenKind::DotAddressSize},
        {".visible",      TokenKind::DotVisible},
        {".entry",        TokenKind::DotEntry},
        {".func",         TokenKind::DotFunc},
        {".param",        TokenKind::DotParam},
        {".reg",          TokenKind::DotReg},
        {".shared",       TokenKind::DotShared},
        {".global",       TokenKind::DotGlobal},
        {".local",        TokenKind::DotLocal},
        {".align",        TokenKind::DotAlign},
        {".byte",         TokenKind::DotByte},
        {".word",         TokenKind::DotWord},
        {".u32",          TokenKind::DotU32},
        {".u64",          TokenKind::DotU64},
        {".s32",          TokenKind::DotS32},
        {".s64",          TokenKind::DotS64},
        {".b8",           TokenKind::DotB8},
        {".b16",          TokenKind::DotB16},
        {".b32",          TokenKind::DotB32},
        {".b64",          TokenKind::DotB64},
        {".u8",           TokenKind::DotU8},
        {".u16",          TokenKind::DotU16},
        {".s8",           TokenKind::DotS8},
        {".s16",          TokenKind::DotS16},
        {".f16",          TokenKind::DotF16},
        {".f32",          TokenKind::DotF32},
        {".f64",          TokenKind::DotF64},
        {".pred",         TokenKind::DotPred},
        {".v2",           TokenKind::DotV2},
        {".v4",           TokenKind::DotV4},
        {".lo",           TokenKind::DotLo},
        {".hi",           TokenKind::DotHi},
        {".wide",         TokenKind::DotWide},
        {".rn",           TokenKind::DotRn},
        {".rz",           TokenKind::DotRz},
        {".rm",           TokenKind::DotRm},
        {".rp",           TokenKind::DotRp},
        {".ftz",          TokenKind::DotFtz},
        {".sat",          TokenKind::DotSat},
        {".eq",           TokenKind::DotEq},
        {".ne",           TokenKind::DotNe},
        {".lt",           TokenKind::DotLt},
        {".le",           TokenKind::DotLe},
        {".gt",           TokenKind::DotGt},
        {".ge",           TokenKind::DotGe},
        {".equ",          TokenKind::DotEqu},
        {".neu",          TokenKind::DotNeu},
        {".ltu",          TokenKind::DotLtu},
        {".leu",          TokenKind::DotLeu},
        {".gtu",          TokenKind::DotGtu},
        {".geu",          TokenKind::DotGeu},
        {".num",          TokenKind::DotNum},
        {".nan",          TokenKind::DotNan},
        {".ls",           TokenKind::DotLs},
        {".hs",           TokenKind::DotHs},
        {".generic",      TokenKind::DotGeneric},
        {".and",          TokenKind::DotAnd},
        {".or",           TokenKind::DotOr},
        {".add",          TokenKind::DotAdd},
        {".xor",          TokenKind::DotXor},
        {".min",          TokenKind::DotMin},
        {".max",          TokenKind::DotMax},
        {".cas",          TokenKind::DotCas},
        {".exch",         TokenKind::DotExch},
        {".to",           TokenKind::DotTo},
        {".sync",         TokenKind::DotSync},
        {".cta",          TokenKind::DotCta},
        {".gpu",          TokenKind::DotGpu},
        {".sys",          TokenKind::DotSys},
    };
    return table;
}

TokenKind Lexer::classifyDotWord(std::string_view word) {
    auto& table = dotKeywords();
    auto it = table.find(std::string(word));
    if (it != table.end()) return it->second;
    // Unknown dot-word → treat as Identifier so the parser can error-recover
    return TokenKind::Identifier;
}

// ============================================================
// Constructor
// ============================================================

Lexer::Lexer(std::string source, std::string filename)
    : src_(std::move(source)), filename_(std::move(filename)) {}

// ============================================================
// Character stream primitives
// ============================================================

char Lexer::peek(int offset) const {
    std::size_t idx = pos_ + static_cast<std::size_t>(offset);
    if (idx >= src_.size()) return '\0';
    return src_[idx];
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; }
    else           { ++col_; }
    return c;
}

// ============================================================
// Skip helpers
// ============================================================

void Lexer::skipLineComment() {
    // Consume everything up to (but not including) the newline
    while (!atEnd() && peek() != '\n') advance();
}

void Lexer::skipBlockComment() {
    // Already consumed '/*'; consume until '*/'
    while (!atEnd()) {
        char c = advance();
        if (c == '*' && peek() == '/') {
            advance(); // consume '/'
            return;
        }
    }
    // EOF inside block comment — we just stop (parser will catch structural issues)
    diagnostics_.push_back(filename_ + ": unterminated block comment");
}

void Lexer::skipWhitespace() {
    while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) advance();
}

// ============================================================
// Number scanner
// ============================================================

Token Lexer::scanNumber() {
    SourceLocation loc = here();
    std::string text;

    // Check for PTX hex-float literal: 0f... or 0d...
    if (peek() == '0' && (peek(1) == 'f' || peek(1) == 'F' || peek(1) == 'd' || peek(1) == 'D')) {
        return scanHexFloat();
    }

    // Hex integer: 0x...
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        text += advance(); text += advance(); // "0x"
        while (!atEnd() && std::isxdigit(static_cast<unsigned char>(peek())))
            text += advance();
        return {TokenKind::IntLiteral, text, loc};
    }

    // Binary integer: 0b...
    if (peek() == '0' && (peek(1) == 'b' || peek(1) == 'B')) {
        text += advance(); text += advance(); // "0b"
        while (!atEnd() && (peek() == '0' || peek() == '1'))
            text += advance();
        return {TokenKind::IntLiteral, text, loc};
    }

    // Decimal / float
    bool isFloat = false;
    while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
        text += advance();

    if (!atEnd() && peek() == '.') {
        // Could be float or struct-access — check next char
        if (std::isdigit(static_cast<unsigned char>(peek(1)))) {
            isFloat = true;
            text += advance(); // '.'
            while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
                text += advance();
        }
    }
    // Exponent
    if (!atEnd() && (peek() == 'e' || peek() == 'E')) {
        isFloat = true;
        text += advance();
        if (!atEnd() && (peek() == '+' || peek() == '-')) text += advance();
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
            text += advance();
    }
    return {isFloat ? TokenKind::FloatLiteral : TokenKind::IntLiteral, text, loc};
}

Token Lexer::scanHexFloat() {
    // PTX hex float: 0f<8 hex digits> (single) or 0d<16 hex digits> (double)
    SourceLocation loc = here();
    std::string text;
    text += advance(); // '0'
    text += advance(); // 'f' or 'd'
    while (!atEnd() && std::isxdigit(static_cast<unsigned char>(peek())))
        text += advance();
    return {TokenKind::FloatLiteral, text, loc};
}

// ============================================================
// String scanner
// ============================================================

Token Lexer::scanString() {
    SourceLocation loc = here();
    std::string text;
    text += advance(); // opening '"'
    while (!atEnd() && peek() != '"') {
        if (peek() == '\\') { text += advance(); } // escape
        text += advance();
    }
    if (!atEnd()) text += advance(); // closing '"'
    return {TokenKind::StringLiteral, text, loc};
}

// ============================================================
// Identifier / keyword scanner
// ============================================================

Token Lexer::scanIdentOrKeyword() {
    SourceLocation loc = here();
    std::string text;
    // Identifiers may start with letter or underscore
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek()))
                        || peek() == '_' || peek() == '$'))
        text += advance();
    return {TokenKind::Identifier, text, loc};
}

// ============================================================
// Dot-word scanner (directive or type qualifier)
// ============================================================

Token Lexer::scanDotWord() {
    SourceLocation loc = here();
    std::string text;
    text += advance(); // '.'
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek()))
                        || peek() == '_'))
        text += advance();
    TokenKind k = classifyDotWord(text);
    return {k, text, loc};
}

// ============================================================
// Register scanner: %name or %name<N>
// ============================================================

Token Lexer::scanRegister() {
    SourceLocation loc = here();
    std::string text;
    text += advance(); // '%'
    // Register name: letters, digits, underscore, dots (for %tid.x etc.)
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek()))
                        || peek() == '_' || peek() == '.'))
        text += advance();
    // Angle-bracket range: %r<10> — consume but keep in text
    if (!atEnd() && peek() == '<') {
        text += advance(); // '<'
        while (!atEnd() && peek() != '>') text += advance();
        if (!atEnd()) text += advance(); // '>'
    }
    return {TokenKind::Register, text, loc};
}

// ============================================================
// Punctuation scanner
// ============================================================

Token Lexer::scanPunct() {
    SourceLocation loc = here();
    char c = advance();
    std::string text(1, c);
    switch (c) {
    case '(': return {TokenKind::LParen,    text, loc};
    case ')': return {TokenKind::RParen,    text, loc};
    case '{': return {TokenKind::LBrace,    text, loc};
    case '}': return {TokenKind::RBrace,    text, loc};
    case '[': return {TokenKind::LBracket,  text, loc};
    case ']': return {TokenKind::RBracket,  text, loc};
    case ',': return {TokenKind::Comma,     text, loc};
    case ';': return {TokenKind::Semicolon, text, loc};
    case ':': return {TokenKind::Colon,     text, loc};
    case '@': return {TokenKind::At,        text, loc};
    case '!': return {TokenKind::Bang,      text, loc};
    case '+': return {TokenKind::Plus,      text, loc};
    case '-': return {TokenKind::Minus,     text, loc};
    case '*': return {TokenKind::Star,      text, loc};
    case '|': return {TokenKind::Pipe,      text, loc};
    case '&': return {TokenKind::Ampersand, text, loc};
    case '^': return {TokenKind::Caret,     text, loc};
    case '~': return {TokenKind::Tilde,     text, loc};
    case '=': return {TokenKind::Equals,    text, loc};
    case '<': return {TokenKind::AngleLt,   text, loc};
    case '>': return {TokenKind::AngleGt,   text, loc};
    default:
        diagnostics_.push_back(loc.toString() + ": unexpected character '" + c + "'");
        return {TokenKind::Unknown, text, loc};
    }
}

// ============================================================
// Main tokenize loop
// ============================================================

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    tokens.reserve(src_.size() / 4); // rough estimate

    while (true) {
        skipWhitespace();
        if (atEnd()) break;

        char c = peek();

        // Comments
        if (c == '/' && peek(1) == '/') {
            skipLineComment();
            continue;
        }
        if (c == '/' && peek(1) == '*') {
            advance(); advance(); // consume '/*'
            skipBlockComment();
            continue;
        }

        // Numbers (including hex/binary)
        if (std::isdigit(static_cast<unsigned char>(c))
            || (c == '-' && std::isdigit(static_cast<unsigned char>(peek(1))))) {
            tokens.push_back(scanNumber());
            continue;
        }

        // Dot-words (directives / type qualifiers)
        if (c == '.') {
            // Peek ahead: if next char is alpha or underscore, it's a keyword
            char next = peek(1);
            if (std::isalpha(static_cast<unsigned char>(next)) || next == '_') {
                tokens.push_back(scanDotWord());
                continue;
            }
            // Bare dot: treat as punct (shouldn't normally appear)
            tokens.push_back(scanPunct());
            continue;
        }

        // Registers start with '%'
        if (c == '%') {
            tokens.push_back(scanRegister());
            continue;
        }

        // Identifiers and keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '$') {
            Token tok = scanIdentOrKeyword();
            // Check if the identifier is immediately followed by ':' → it's a label
            skipWhitespace();
            if (!atEnd() && peek() == ':') {
                // Consume the colon; the label token includes the name but not the colon
                // We emit the label name as Identifier and the colon separately so the
                // parser can detect "identifier followed by colon" as a label definition.
                tokens.push_back(tok); // Identifier
                tokens.push_back({TokenKind::Colon, ":", here()});
                advance(); // consume ':'
                continue;
            }
            tokens.push_back(tok);
            continue;
        }

        // String literals
        if (c == '"') {
            tokens.push_back(scanString());
            continue;
        }

        // All other single/double-char punctuation
        tokens.push_back(scanPunct());
    }

    tokens.push_back({TokenKind::Eof, "", here()});
    return tokens;
}

Token Lexer::makeToken(TokenKind k, std::string text, SourceLocation loc) const {
    return {k, std::move(text), std::move(loc)};
}

} // namespace ptxra
