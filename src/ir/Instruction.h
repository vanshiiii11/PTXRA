#pragma once
#include "Operand.h"
#include "../ast/AST.h"
#include <vector>
#include <optional>
#include <string>
#include <cstdint>

namespace ptxra {
namespace ir {

/// IR opcode, closely mapping PTX opcodes.
enum class IrOpcode {
    Mov,
    Add, Sub, Mul, Mad, Fma,
    Div, Rem, Neg, Abs, Min, Max,
    And, Or, Xor, Not,
    Shl, Shr,
    Cvt, Cvta,
    Setp, Selp,
    Ld, St,
    Bra,     // unconditional branch
    CbrTrue, // conditional branch (predicate is true)
    CbrFalse,// conditional branch (predicate is false)
    Ret,
    Exit,
    BarSync,
    Atom,
    // Spill helpers (inserted by allocator)
    SpillStore, // st.local [slot], vreg
    SpillLoad,  // ld.local vreg, [slot]
    Unknown,
};

std::string iropcName(IrOpcode op);

/// Address space for memory instructions
using ast::AddrSpace;

/// A single IR instruction.
struct Instruction {
    IrOpcode opcode    = IrOpcode::Unknown;
    IrType   type      = IrType::Unknown;      // result/operand type
    AddrSpace space    = AddrSpace::None;       // for ld/st
    ast::CmpOp cmpOp  = ast::CmpOp::None;     // for setp

    // Modifiers
    bool lo   = false;
    bool hi   = false;
    bool wide = false;

    // Predicate guard (stored as a use of a pred vreg)
    std::optional<VRegId> predReg;  // the predicate register
    bool                  predNeg = false; // true if @!%p

    // Destinations and sources
    std::vector<Operand> defs; // defined (written) operands
    std::vector<Operand> uses; // used (read) operands

    // For branch instructions
    std::string branchTarget; // label name

    // Source location (for diagnostics)
    uint32_t srcLine = 0;

    // Spill slot index (set by SpillStore/SpillLoad)
    int spillSlot = -1;

    /// Return all VRegIds defined by this instruction.
    std::vector<VRegId> defRegs() const;
    /// Return all VRegIds used by this instruction (including predicate).
    std::vector<VRegId> useRegs() const;

    bool isBranch()    const { return opcode == IrOpcode::Bra
                                   || opcode == IrOpcode::CbrTrue
                                   || opcode == IrOpcode::CbrFalse; }
    bool isTerminator()const { return isBranch()
                                   || opcode == IrOpcode::Ret
                                   || opcode == IrOpcode::Exit; }

    std::string toString() const;
};

} // namespace ir
} // namespace ptxra
