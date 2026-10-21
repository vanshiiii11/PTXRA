#pragma once
#include "SourceLocation.h"
#include <string>
#include <string_view>

namespace ptxra {

/// Every terminal symbol produced by the lexer.
enum class TokenKind {
    // Literals
    IntLiteral,     // 42, 0xff, 0b1010
    FloatLiteral,   // 3.14, 0f3F800000
    StringLiteral,  // "hello"

    // Identifiers and keywords
    Identifier,     // generic name
    Register,       // %r5, %f0, %p1, %rd2, %tid, etc.
    Label,          // name: (includes the colon)
    LabelRef,       // branch target (plain identifier in branch context; resolved by parser)

    // Directives (all start with '.')
    DotVersion,         // .version
    DotTarget,          // .target
    DotAddressSize,     // .address_size
    DotVisible,         // .visible
    DotEntry,           // .entry
    DotFunc,            // .func
    DotParam,           // .param
    DotReg,             // .reg
    DotShared,          // .shared
    DotGlobal,          // .global
    DotLocal,           // .local
    DotAlign,           // .align
    DotByte,            // .byte (data initializer)
    DotWord,            // .word
    DotU32,             // type directives used inside .reg/.param
    DotU64,
    DotS32,
    DotS64,
    DotB8, DotB16, DotB32, DotB64,
    DotU8,  DotU16,
    DotS8,  DotS16,
    DotF16, DotF32, DotF64,
    DotPred,            // .pred
    DotV2, DotV4,       // vector types
    DotLo, DotHi, DotWide, // mul modifiers
    DotRn, DotRz, DotRm, DotRp, // rounding modes
    DotFtz,             // flush-to-zero
    DotSat,             // saturate
    DotEq, DotNe, DotLt, DotLe, DotGt, DotGe,  // setp comparison
    DotEqu, DotNeu, DotLtu, DotLeu, DotGtu, DotGeu, // unordered
    DotNum, DotNan,     // setp ordered/unordered
    DotLs, DotHs,       // unsigned lt/ge for setp
    DotGlobal_space,    // .global as address-space qualifier (duplicated for clarity)
    DotShared_space,    // .shared as address-space qualifier
    DotParam_space,     // .param as address-space qualifier
    DotLocal_space,     // .local as address-space qualifier
    DotGeneric,         // .generic
    DotAnd,             // .and atom modifier
    DotOr,              // .or atom modifier
    DotAdd,             // .add atom modifier
    DotXor,             // .xor atom modifier
    DotMin,             // .min atom modifier
    DotMax,             // .max atom modifier
    DotCas,             // .cas atom modifier
    DotExch,            // .exch atom modifier
    DotTo,              // .to cvta modifier
    DotSync,            // .sync (bar.sync)
    DotCta,             // .cta (scope)
    DotGpu,
    DotSys,

    // Punctuation / symbols
    LParen,     // (
    RParen,     // )
    LBrace,     // {
    RBrace,     // }
    LBracket,   // [
    RBracket,   // ]
    Comma,      // ,
    Semicolon,  // ;
    Colon,      // :
    At,         // @ (predicate prefix)
    Bang,       // ! (predicate negation)
    Plus,       // +
    Minus,      // -
    Star,       // *
    Pipe,       // |
    Ampersand,  // &
    Caret,      // ^
    Tilde,      // ~
    Equals,     // =
    Lt,         // <
    Gt,         // >
    AngleLt,    // < (in %r<10>)
    AngleGt,    // >

    // Special / meta
    Eof,
    Unknown,    // illegal character — lexer emits this and continues
};

/// Human-readable name for a TokenKind (for diagnostics).
std::string_view tokenKindName(TokenKind k);

/// A single lexed token.
struct Token {
    TokenKind      kind     = TokenKind::Unknown;
    std::string    text;           // raw source spelling
    SourceLocation loc;

    // Convenience constructors
    Token() = default;
    Token(TokenKind k, std::string t, SourceLocation l)
        : kind(k), text(std::move(t)), loc(std::move(l)) {}

    bool is(TokenKind k)    const { return kind == k; }
    bool isNot(TokenKind k) const { return kind != k; }
    bool isEof()            const { return kind == TokenKind::Eof; }

    /// True for any type-qualifier dot token (.b32, .f32, .pred, …)
    bool isTypeQualifier() const;
    /// True for any address-space dot token (.global, .shared, .param, .local)
    bool isAddressSpace()  const;
    /// True for any setp comparison modifier (.eq, .ne, .lt, …)
    bool isCmpOp()         const;
};

} // namespace ptxra
