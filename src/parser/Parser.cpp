#include "Parser.h"
#include "../support/StringUtil.h"
#include <cassert>
#include <sstream>
#include <unordered_set>

namespace ptxra {

using ast::PtxType;
using ast::AddrSpace;
using ast::CmpOp;
using ast::Opcode;
using ast::InsnMods;

// ============================================================
// Constructor
// ============================================================

Parser::Parser(std::vector<Token> tokens, std::string filename)
    : tokens_(std::move(tokens)), filename_(std::move(filename)) {}

// ============================================================
// Token stream helpers
// ============================================================

const Token& Parser::peek(int offset) const {
    std::size_t idx = pos_ + static_cast<std::size_t>(offset);
    if (idx >= tokens_.size()) return tokens_.back(); // Eof
    return tokens_[idx];
}

Token Parser::consume() {
    Token t = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) ++pos_;
    return t;
}

Token Parser::expect(TokenKind k) {
    if (!check(k)) {
        throw ParseError("expected " + std::string(tokenKindName(k))
                         + " but got '" + current().text + "'", here());
    }
    return consume();
}

bool Parser::check(TokenKind k) const {
    return current().kind == k;
}

bool Parser::match(TokenKind k) {
    if (check(k)) { consume(); return true; }
    return false;
}

bool Parser::atEnd() const {
    return current().isEof();
}

std::string Parser::errorContext() const {
    return " (near '" + current().text + "' at " + here().toString() + ")";
}

// ============================================================
// Top-level parse
// ============================================================

ast::Module Parser::parse() {
    ast::Module mod;
    mod.filename = filename_;

    // Mandatory header directives
    // .version N.M
    expect(TokenKind::DotVersion);
    // Version number can be "7.5" or "7" depending on PTX
    if (check(TokenKind::FloatLiteral) || check(TokenKind::IntLiteral)) {
        mod.version = consume().text;
    } else {
        throw ParseError("expected version number after .version", here());
    }

    // .target sm_XX [, ...] or just sm_XX
    expect(TokenKind::DotTarget);
    mod.target = consume().text; // "sm_80" etc.
    // optional comma-separated list (e.g., "sm_80, debug") — skip
    while (match(TokenKind::Comma)) consume();

    // .address_size 32|64 (optional)
    if (match(TokenKind::DotAddressSize)) {
        mod.addressSize = std::stoi(consume().text);
    }

    // Now parse globals and kernels in any order
    while (!atEnd()) {
        bool isVisible = match(TokenKind::DotVisible);

        if (check(TokenKind::DotEntry)) {
            consume();
            mod.kernels.push_back(parseKernel(/*isEntry=*/true, isVisible));
        } else if (check(TokenKind::DotFunc)) {
            consume();
            mod.kernels.push_back(parseKernel(/*isEntry=*/false, isVisible));
        } else if (check(TokenKind::DotGlobal) || check(TokenKind::DotShared)) {
            mod.globals.push_back(parseGlobal());
        } else if (check(TokenKind::DotAlign) || check(TokenKind::DotByte)
                   || check(TokenKind::DotWord)) {
            // top-level data — skip for now
            while (!atEnd() && !check(TokenKind::Semicolon)) consume();
            match(TokenKind::Semicolon);
        } else if (atEnd()) {
            break;
        } else {
            // Unknown top-level token — skip to recover
            auto loc = here();
            (void)loc;
            consume();
        }
    }

    return mod;
}

// ============================================================
// Global variable
// ============================================================

ast::GlobalVar Parser::parseGlobal() {
    ast::GlobalVar g;
    g.loc = here();

    if (check(TokenKind::DotGlobal))  { g.space = AddrSpace::Global; consume(); }
    else if (check(TokenKind::DotShared)) { g.space = AddrSpace::Shared; consume(); }
    else if (check(TokenKind::DotLocal))  { g.space = AddrSpace::Local; consume(); }

    // optional .align N
    if (match(TokenKind::DotAlign)) {
        g.align = std::stoi(consume().text);
    }

    // type
    if (current().isTypeQualifier()) {
        g.type = ast::ptxTypeFromDotToken(current().text);
        consume();
    }

    // name
    g.name = expect(TokenKind::Identifier).text;

    // optional array size [N]
    if (match(TokenKind::LBracket)) {
        if (check(TokenKind::IntLiteral)) g.count = std::stoi(consume().text);
        expect(TokenKind::RBracket);
    }

    match(TokenKind::Semicolon);
    return g;
}

// ============================================================
// Kernel / function
// ============================================================

ast::Kernel Parser::parseKernel(bool isEntry, bool isVisible) {
    ast::Kernel k;
    k.isEntry   = isEntry;
    k.isVisible = isVisible;
    k.loc       = here();

    // Optional return type for .func (skip for now)
    if (!isEntry && check(TokenKind::LParen)) {
        consume(); // '('
        while (!check(TokenKind::RParen) && !atEnd()) consume();
        expect(TokenKind::RParen);
    }

    k.name = expect(TokenKind::Identifier).text;

    // Parameter list
    expect(TokenKind::LParen);
    while (!check(TokenKind::RParen) && !atEnd()) {
        k.params.push_back(parseParam());
        if (!match(TokenKind::Comma)) break;
    }
    expect(TokenKind::RParen);

    // Body
    expect(TokenKind::LBrace);
    // Declarations (.reg, .shared, .local, .align) in any order
    while (check(TokenKind::DotReg) || check(TokenKind::DotShared)
           || check(TokenKind::DotLocal) || check(TokenKind::DotAlign)) {
        if (check(TokenKind::DotReg)) {
            auto decls = parseRegBlock();
            k.regDecls.insert(k.regDecls.end(), decls.begin(), decls.end());
        } else {
            while (!check(TokenKind::Semicolon) && !atEnd()) consume();
            match(TokenKind::Semicolon);
        }
    }

    // Instructions
    while (!check(TokenKind::RBrace) && !atEnd()) {
        k.body.push_back(parseStmt());
    }
    expect(TokenKind::RBrace);

    return k;
}

// ============================================================
// Parameter
// ============================================================

ast::Param Parser::parseParam() {
    ast::Param p;
    p.loc = here();

    // .param (already known — caller checks context)
    if (match(TokenKind::DotParam)) {
        p.space = AddrSpace::Param;
    }

    // .align N
    if (match(TokenKind::DotAlign)) {
        p.align = std::stoi(consume().text);
    }

    // type
    if (current().isTypeQualifier()) {
        p.type = ast::ptxTypeFromDotToken(current().text);
        consume();
    }

    // name
    if (check(TokenKind::Identifier) || check(TokenKind::Register)) {
        p.name = consume().text;
    } else {
        throw ParseError("expected parameter name, got '" + current().text + "'", here());
    }

    return p;
}

// ============================================================
// Register declarations  .reg .type %name<N>;
// ============================================================

std::vector<ast::RegDecl> Parser::parseRegBlock() {
    std::vector<ast::RegDecl> decls;
    expect(TokenKind::DotReg);

    ast::RegDecl rd;
    rd.loc = here();

    // type
    if (!current().isTypeQualifier()) {
        throw ParseError("expected type qualifier after .reg", here());
    }
    rd.type = ast::ptxTypeFromDotToken(current().text);
    consume();

    // One or more register names separated by commas
    do {
        ast::RegDecl d = rd;
        d.name = expect(TokenKind::Register).text;
        d.count = 1;

        // The name may already contain the angle-bracket range (from lexer):
        // e.g., "%r<10>" → name="%r<10>", count=10
        auto& nm = d.name;
        auto lt = nm.find('<');
        if (lt != std::string::npos) {
            auto gt = nm.find('>');
            if (gt != std::string::npos) {
                d.count = std::stoi(nm.substr(lt + 1, gt - lt - 1));
                nm = nm.substr(0, lt); // strip angle-bracket part from name
            }
        }

        decls.push_back(d);
    } while (match(TokenKind::Comma));

    expect(TokenKind::Semicolon);
    return decls;
}

// ============================================================
// Statement: optional label + instruction
// ============================================================

ast::Stmt Parser::parseStmt() {
    ast::Stmt st;

    // A label is "Identifier followed by Colon"
    if (check(TokenKind::Identifier) && peek(1).is(TokenKind::Colon)) {
        ast::LabelDef ld;
        ld.name = consume().text; // identifier
        ld.loc  = here();
        consume(); // colon
        st.label = ld;

        // A label can be followed by another label or an instruction
        if (check(TokenKind::RBrace) || atEnd()) return st;
    }

    // Parse instruction if there's something other than '}' or another label
    if (!check(TokenKind::RBrace) && !atEnd()
        && !(check(TokenKind::Identifier) && peek(1).is(TokenKind::Colon))) {
        try {
            st.insn = parseInstruction();
            expect(TokenKind::Semicolon);
        } catch (const UnsupportedFeature& uf) {
            // Skip to semicolon and continue — emit warning
            // (caller may choose to log diagnostics)
            while (!check(TokenKind::Semicolon) && !atEnd()) consume();
            match(TokenKind::Semicolon);
            throw; // re-throw so CLI can optionally report
        }
    }

    return st;
}

// ============================================================
// Instruction
// ============================================================

ast::Instruction Parser::parseInstruction() {
    ast::Instruction insn;
    insn.loc = here();

    // Predicate guard: @%p or @!%p
    if (match(TokenKind::At)) {
        ast::Predicate pred;
        pred.negate = match(TokenKind::Bang);
        pred.reg    = expect(TokenKind::Register).text;
        insn.pred   = pred;
    }

    // Opcode mnemonic (may be compound like "bar" then ".sync")
    if (!check(TokenKind::Identifier)) {
        throw ParseError("expected instruction mnemonic", here());
    }
    std::string mnemonic = consume().text;

    // Handle bar.sync specially: mnemonic="bar", next token=".sync"
    if (mnemonic == "bar" && check(TokenKind::DotSync)) {
        consume(); // ".sync"
        insn.opcode = Opcode::BarSync;
        // Operand: barrier id (integer)
        if (!check(TokenKind::Semicolon)) {
            insn.srcs.push_back(parseOperand());
        }
        return insn;
    }

    insn.opcode = ast::opcodeFromString(mnemonic);
    if (insn.opcode == Opcode::Unknown) {
        throw UnsupportedFeature("unknown opcode '" + mnemonic + "'", insn.loc);
    }

    // Parse modifiers (all leading dot-tokens before the first register/literal)
    insn.mods = parseMods(insn.opcode);

    // Parse destinations and sources
    // Different instructions have different dest/src counts:
    switch (insn.opcode) {
    // 0 dest, 0 src
    case Opcode::Ret:
    case Opcode::Exit:
        break;

    // 0 dest, 1 src (branch target)
    case Opcode::Bra:
        // Target is an identifier (label)
        if (check(TokenKind::Identifier)) {
            ast::Operand op;
            op.loc = here();
            op.value = ast::LabelOperand{consume().text};
            insn.srcs.push_back(op);
        }
        break;

    // Store: st.space.type [addr], src
    case Opcode::St: {
        insn.dests.push_back(parseAddrExpr());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        break;
    }

    // Load: ld.space.type dest, [addr]
    case Opcode::Ld: {
        insn.dests.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseAddrExpr());
        break;
    }

    // setp.cmp.type p, a, b  (dest is predicate)
    case Opcode::Setp: {
        insn.dests.push_back(parseOperand());
        // setp can write two predicates: p|q
        if (match(TokenKind::Pipe)) {
            insn.dests.push_back(parseOperand());
        }
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        break;
    }

    // selp: dest, a, b, p
    case Opcode::Selp: {
        insn.dests.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        break;
    }

    // mad/fma: dest, a, b, c
    case Opcode::Mad:
    case Opcode::Fma: {
        insn.dests.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        break;
    }

    // neg/abs/not/cvt/cvta/mov: dest, src
    case Opcode::Neg:
    case Opcode::Abs:
    case Opcode::Not:
    case Opcode::Cvt:
    case Opcode::Cvta:
    case Opcode::Mov: {
        insn.dests.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        break;
    }

    // atom: dest, [addr], src
    case Opcode::Atom: {
        insn.dests.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseAddrExpr());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        break;
    }

    // Default binary: dest, src1, src2
    default: {
        insn.dests.push_back(parseOperand());
        expect(TokenKind::Comma);
        insn.srcs.push_back(parseOperand());
        // Try to parse second source if comma follows
        if (match(TokenKind::Comma)) {
            insn.srcs.push_back(parseOperand());
        }
        break;
    }
    }

    return insn;
}

// ============================================================
// Modifier parsing
// ============================================================

ast::InsnMods Parser::parseMods(Opcode /*opcode*/) {
    InsnMods mods;
    bool parsedType = false;

    while (true) {
        const Token& t = current();

        // Address space modifiers
        if (t.isAddressSpace()) {
            switch (t.kind) {
            case TokenKind::DotGlobal: case TokenKind::DotGlobal_space:
                mods.space = AddrSpace::Global; break;
            case TokenKind::DotShared: case TokenKind::DotShared_space:
                mods.space = AddrSpace::Shared; break;
            case TokenKind::DotParam:  case TokenKind::DotParam_space:
                mods.space = AddrSpace::Param;  break;
            case TokenKind::DotLocal:  case TokenKind::DotLocal_space:
                mods.space = AddrSpace::Local;  break;
            case TokenKind::DotGeneric:
                mods.space = AddrSpace::Generic; break;
            default: break;
            }
            consume(); continue;
        }

        // Width modifiers
        if (t.kind == TokenKind::DotLo)   { mods.lo   = true; consume(); continue; }
        if (t.kind == TokenKind::DotHi)   { mods.hi   = true; consume(); continue; }
        if (t.kind == TokenKind::DotWide) { mods.wide = true; consume(); continue; }

        // Comparison modifier (setp)
        if (t.isCmpOp()) {
            mods.cmpOp = tokenToCmpOp(t);
            consume(); continue;
        }

        // Rounding
        if (t.kind == TokenKind::DotRn) { mods.round = ast::RoundMode::Rn; consume(); continue; }
        if (t.kind == TokenKind::DotRz) { mods.round = ast::RoundMode::Rz; consume(); continue; }
        if (t.kind == TokenKind::DotRm) { mods.round = ast::RoundMode::Rm; consume(); continue; }
        if (t.kind == TokenKind::DotRp) { mods.round = ast::RoundMode::Rp; consume(); continue; }

        // Flush-to-zero / saturate
        if (t.kind == TokenKind::DotFtz) { mods.ftz = true; consume(); continue; }
        if (t.kind == TokenKind::DotSat) { mods.sat = true; consume(); continue; }

        // .to (cvta.to)
        if (t.kind == TokenKind::DotTo) { mods.toSpace = true; consume(); continue; }

        // Atom op modifiers
        if (t.kind == TokenKind::DotAdd) { mods.atomOp = "add"; consume(); continue; }
        if (t.kind == TokenKind::DotOr)  { mods.atomOp = "or";  consume(); continue; }
        if (t.kind == TokenKind::DotAnd) { mods.atomOp = "and"; consume(); continue; }
        if (t.kind == TokenKind::DotXor) { mods.atomOp = "xor"; consume(); continue; }
        if (t.kind == TokenKind::DotMin) { mods.atomOp = "min"; consume(); continue; }
        if (t.kind == TokenKind::DotMax) { mods.atomOp = "max"; consume(); continue; }
        if (t.kind == TokenKind::DotCas) { mods.atomOp = "cas"; consume(); continue; }
        if (t.kind == TokenKind::DotExch){ mods.atomOp = "exch";consume(); continue; }

        // Sync modifier
        if (t.kind == TokenKind::DotSync) { consume(); continue; }

        // Type qualifier
        if (t.isTypeQualifier()) {
            PtxType ty = ast::ptxTypeFromDotToken(t.text);
            if (!parsedType) {
                mods.type = ty;
                parsedType = true;
            } else {
                // Second type → source type for cvt
                mods.srcType = ty;
            }
            consume(); continue;
        }

        // Nothing matched → stop
        break;
    }
    return mods;
}

// ============================================================
// Operand parsers
// ============================================================

ast::Operand Parser::parseOperand() {
    ast::Operand op;
    op.loc = here();

    if (check(TokenKind::Register)) {
        std::string name = consume().text;
        // Parse the register name: "%r5" → {name:"%r", index:5}
        // or "%r<5>" → already expanded by lexer to "%r<5>"
        // We normalize it: strip trailing digits to get base name, index from digits
        // But registers like %tid.x, %ntid.x are special names
        ast::RegOperand ro;
        ro.index = -1;

        // If name contains '<' it's a range placeholder — unusual in instruction context
        if (name.find('<') != std::string::npos) {
            ro.name  = name;
            ro.index = -1;
        } else {
            // Split trailing digits: "%r15" → base="%r", index=15
            std::size_t i = name.size();
            while (i > 0 && std::isdigit(static_cast<unsigned char>(name[i-1]))) --i;
            if (i < name.size() && i > 0) {
                ro.name  = name.substr(0, i);
                ro.index = std::stoi(name.substr(i));
            } else {
                ro.name  = name;
                ro.index = -1;
            }
        }
        op.value = ro;
    } else if (check(TokenKind::IntLiteral) || check(TokenKind::FloatLiteral)) {
        op.value = ast::ImmOperand{consume().text};
    } else if (check(TokenKind::Minus)) {
        // Negative literal
        std::string text = consume().text; // '-'
        if (check(TokenKind::IntLiteral) || check(TokenKind::FloatLiteral)) {
            text += consume().text;
        }
        op.value = ast::ImmOperand{text};
    } else if (check(TokenKind::Identifier)) {
        // Label reference (e.g., branch target)
        op.value = ast::LabelOperand{consume().text};
    } else if (check(TokenKind::LBracket)) {
        op = parseAddrExpr();
    } else {
        throw ParseError("expected operand but got '" + current().text + "'", here());
    }

    return op;
}

ast::Operand Parser::parseAddrExpr() {
    ast::Operand op;
    op.loc = here();
    expect(TokenKind::LBracket);

    ast::AddrOperand addr;

    // base: register, identifier, or immediate offset
    if (check(TokenKind::Register)) {
        addr.base = consume().text;
    } else if (check(TokenKind::Identifier)) {
        addr.base = consume().text;
    } else if (check(TokenKind::IntLiteral)) {
        addr.base = "0";
        addr.offset = std::stoll(consume().text);
    } else {
        throw ParseError("expected address base in [...]", here());
    }

    // optional offset: +N or -N
    if (check(TokenKind::Plus)) {
        consume();
        addr.offset = std::stoll(expect(TokenKind::IntLiteral).text);
    } else if (check(TokenKind::Minus)) {
        consume();
        addr.offset = -std::stoll(expect(TokenKind::IntLiteral).text);
    }

    expect(TokenKind::RBracket);
    op.value = addr;
    return op;
}

// ============================================================
// Comparison operator mapping
// ============================================================

CmpOp Parser::tokenToCmpOp(const Token& t) {
    switch (t.kind) {
    case TokenKind::DotEq:  return CmpOp::Eq;
    case TokenKind::DotNe:  return CmpOp::Ne;
    case TokenKind::DotLt:  return CmpOp::Lt;
    case TokenKind::DotLe:  return CmpOp::Le;
    case TokenKind::DotGt:  return CmpOp::Gt;
    case TokenKind::DotGe:  return CmpOp::Ge;
    case TokenKind::DotEqu: return CmpOp::Equ;
    case TokenKind::DotNeu: return CmpOp::Neu;
    case TokenKind::DotLtu: return CmpOp::Ltu;
    case TokenKind::DotLeu: return CmpOp::Leu;
    case TokenKind::DotGtu: return CmpOp::Gtu;
    case TokenKind::DotGeu: return CmpOp::Geu;
    case TokenKind::DotNum: return CmpOp::Num;
    case TokenKind::DotNan: return CmpOp::Nan;
    case TokenKind::DotLs:  return CmpOp::Ls;
    case TokenKind::DotHs:  return CmpOp::Hs;
    default:                return CmpOp::None;
    }
}

} // namespace ptxra
