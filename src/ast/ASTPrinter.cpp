#include "ASTPrinter.h"
#include <sstream>

namespace ptxra {
namespace ast {

// ============================================================
// Helper: build modifier string for an instruction
// ============================================================

std::string ASTPrinter::printMods(const InsnMods& m, Opcode opcode) {
    std::string s;
    // Address space (ld/st/cvta)
    if (m.space != AddrSpace::None) s += addrSpaceName(m.space);
    // toSpace modifier for cvta
    if (m.toSpace) s += ".to";
    // Width modifiers (mul, mad)
    if (m.lo)   s += ".lo";
    if (m.hi)   s += ".hi";
    if (m.wide) s += ".wide";
    // Atom op
    if (!m.atomOp.empty()) s += "." + m.atomOp;
    // Comparison (setp)
    if (m.cmpOp != CmpOp::None) s += cmpOpName(m.cmpOp);
    // Rounding
    if (m.round != RoundMode::None) s += roundModeName(m.round);
    // Flush-to-zero
    if (m.ftz) s += ".ftz";
    // Saturate
    if (m.sat) s += ".sat";
    // Main type
    if (m.type != PtxType::Unknown) s += ptxTypeName(m.type);
    // Source type (cvt only)
    if (opcode == Opcode::Cvt && m.srcType != PtxType::Unknown)
        s += ptxTypeName(m.srcType);
    return s;
}

// ============================================================
// Module
// ============================================================

void ASTPrinter::print(const Module& mod) {
    emit(".version " + mod.version); newline();
    emit(".target "  + mod.target);  newline();
    emit(".address_size " + std::to_string(mod.addressSize)); newline();
    newline();
    for (const auto& g : mod.globals) print(g);
    for (const auto& k : mod.kernels) print(k);
}

// ============================================================
// Global variable
// ============================================================

void ASTPrinter::print(const GlobalVar& g) {
    emit(addrSpaceName(g.space));
    if (g.align > 0) emit(" .align " + std::to_string(g.align));
    emit(ptxTypeName(g.type));
    emit(" " + g.name);
    if (g.count > 1) emit("[" + std::to_string(g.count) + "]");
    emit(";"); newline();
}

// ============================================================
// Kernel
// ============================================================

void ASTPrinter::print(const Kernel& k) {
    if (k.isVisible) emit(".visible ");
    emit(k.isEntry ? ".entry " : ".func ");
    emit(k.name);

    // Parameters
    emit("("); newline();
    for (std::size_t i = 0; i < k.params.size(); ++i) {
        emit("    ");
        print(k.params[i]);
        if (i + 1 < k.params.size()) emit(",");
        newline();
    }
    emit(")"); newline();
    emit("{"); newline();
    indent_++;

    // Register declarations
    for (const auto& rd : k.regDecls) {
        ind(); print(rd); newline();
    }
    if (!k.regDecls.empty()) newline();

    // Body statements
    for (const auto& st : k.body) {
        print(st);
    }

    indent_--;
    emit("}"); newline(); newline();
}

// ============================================================
// Param
// ============================================================

void ASTPrinter::print(const Param& p) {
    emit(".param ");
    if (p.align > 0) emit(".align " + std::to_string(p.align) + " ");
    emit(ptxTypeName(p.type));
    emit(" " + p.name);
}

// ============================================================
// Register declaration
// ============================================================

void ASTPrinter::print(const RegDecl& rd) {
    emit(".reg ");
    emit(ptxTypeName(rd.type));
    emit(" " + rd.name);
    if (rd.count > 1) {
        // Strip trailing '<N>' if present and re-add canonical form
        std::string base = rd.name;
        auto pos = base.find('<');
        if (pos != std::string::npos) base = base.substr(0, pos);
        emit("<" + std::to_string(rd.count) + ">");
    }
    emit(";");
}

// ============================================================
// Statement (may have a label prefix)
// ============================================================

void ASTPrinter::print(const Stmt& st) {
    if (st.label) {
        emit(st.label->name + ":"); newline();
    }
    if (st.insn) {
        ind();
        print(*st.insn);
        emit(";"); newline();
    }
}

// ============================================================
// Instruction
// ============================================================

void ASTPrinter::print(const Instruction& insn) {
    // Predicate guard
    if (insn.pred) {
        emit("@");
        if (insn.pred->negate) emit("!");
        emit(insn.pred->reg + " ");
    }

    // Mnemonic + modifiers
    std::string mnemonic;
    if (insn.opcode == Opcode::BarSync) {
        mnemonic = "bar.sync";
    } else {
        mnemonic = opcodeName(insn.opcode);
    }
    mnemonic += printMods(insn.mods, insn.opcode);
    emit(mnemonic);

    // Destinations
    if (!insn.dests.empty()) {
        emit(" ");
        for (std::size_t i = 0; i < insn.dests.size(); ++i) {
            if (i > 0) emit("|");
            print(insn.dests[i]);
        }
        if (!insn.srcs.empty()) emit(", ");
    } else {
        if (!insn.srcs.empty()) emit(" ");
    }

    // Sources
    for (std::size_t i = 0; i < insn.srcs.size(); ++i) {
        if (i > 0) emit(", ");
        print(insn.srcs[i]);
    }
}

// ============================================================
// Operand
// ============================================================

void ASTPrinter::print(const Operand& op) {
    if (op.isReg()) {
        const auto& r = op.asReg();
        emit(r.name);
        if (r.index >= 0) emit(std::to_string(r.index));
    } else if (op.isImm()) {
        emit(op.asImm().raw);
    } else if (op.isLabel()) {
        emit(op.asLabel().name);
    } else if (op.isAddr()) {
        const auto& a = op.asAddr();
        emit("[" + a.base);
        if (a.offset) {
            if (*a.offset >= 0) emit("+" + std::to_string(*a.offset));
            else                 emit(std::to_string(*a.offset));
        }
        emit("]");
    }
}

} // namespace ast
} // namespace ptxra
