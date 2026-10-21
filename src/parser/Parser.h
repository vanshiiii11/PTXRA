#pragma once
#include "../lexer/Token.h"
#include "../ast/AST.h"
#include "ParseError.h"
#include <vector>
#include <string>

namespace ptxra {

/// Recursive-descent parser for PTX source.
///
/// Consumes the token stream produced by Lexer and builds an ast::Module.
/// Errors throw ParseError with file:line:col information.
/// Unsupported instructions throw UnsupportedFeature rather than crashing.
class Parser {
public:
    /// @param tokens  Token stream (must end with Eof)
    /// @param filename Used in SourceLocation for diagnostics
    explicit Parser(std::vector<Token> tokens, std::string filename = "<input>");

    /// Parse the entire module and return it.
    ast::Module parse();

private:
    std::vector<Token> tokens_;
    std::size_t        pos_ = 0;
    std::string        filename_;

    // ---- Token stream helpers ----
    const Token& peek(int offset = 0) const;
    const Token& current() const { return peek(0); }
    Token        consume();                         // advance and return previous
    Token        expect(TokenKind k);               // consume or throw
    bool         check(TokenKind k) const;
    bool         match(TokenKind k);                // consume if matches
    bool         atEnd() const;
    SourceLocation here() const { return current().loc; }

    // ---- Top-level parsers ----
    ast::GlobalVar  parseGlobal();
    ast::Kernel     parseKernel(bool isEntry, bool isVisible);
    ast::Param      parseParam();
    ast::RegDecl    parseRegDecl();
    ast::Stmt       parseStmt();
    ast::Instruction parseInstruction();

    // ---- Operand parsers ----
    ast::Operand    parseOperand();
    ast::Operand    parseAddrExpr();    // [base+offset]

    // ---- Modifier parsers (return false if token not consumed) ----
    bool parseTypeModifier(ast::InsnMods& mods, ast::Opcode op);
    bool parseSpaceModifier(ast::InsnMods& mods);
    bool parseCmpModifier(ast::InsnMods& mods);
    bool parseRoundModifier(ast::InsnMods& mods);

    // ---- Helpers ----
    ast::PtxType   expectType();   // .pred/.u32/etc. or throw
    ast::AddrSpace expectSpace();  // .global/.shared/etc. or throw
    ast::CmpOp     tokenToCmpOp(const Token& t);

    /// Reads all modifier dots preceding the type qualifier for an instruction.
    /// Sets mods fields and returns the type.
    ast::InsnMods parseMods(ast::Opcode opcode);

    /// Parse the register declaration inside a kernel body.
    std::vector<ast::RegDecl> parseRegBlock();

    std::string errorContext() const;
};

} // namespace ptxra
