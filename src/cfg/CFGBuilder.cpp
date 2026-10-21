#include "CFGBuilder.h"
#include "../parser/ParseError.h"
#include <cassert>
#include <stdexcept>
#include <unordered_set>

namespace ptxra {
namespace cfg {

using ir::IrType;
using ir::IrOpcode;
using ir::VRegId;
using ir::InvalidVReg;
using ir::BasicBlock;
using ir::Function;
using ir::BlockId;

using ast::PtxType;
using ast::AddrSpace;
using ast::CmpOp;

// ============================================================
// Type mapping
// ============================================================

IrType CFGBuilder::mapType(PtxType t) {
    switch (t) {
    case PtxType::Pred:                         return IrType::Pred;
    case PtxType::B8:  case PtxType::U8:  case PtxType::S8:
    case PtxType::B16: case PtxType::U16: case PtxType::S16:
    case PtxType::B32: case PtxType::U32: case PtxType::S32: return IrType::Int32;
    case PtxType::B64: case PtxType::U64: case PtxType::S64: return IrType::Int64;
    case PtxType::F16: case PtxType::F32:                    return IrType::Float32;
    case PtxType::F64:                                        return IrType::Float64;
    case PtxType::Unknown:                                    return IrType::Unknown;
    }
    return IrType::Unknown;
}

// ============================================================
// Opcode mapping (1-to-1 where possible)
// ============================================================

IrOpcode CFGBuilder::mapOpcode(ast::Opcode op) {
    switch (op) {
    case ast::Opcode::Mov:     return IrOpcode::Mov;
    case ast::Opcode::Add:     return IrOpcode::Add;
    case ast::Opcode::Sub:     return IrOpcode::Sub;
    case ast::Opcode::Mul:     return IrOpcode::Mul;
    case ast::Opcode::Mad:     return IrOpcode::Mad;
    case ast::Opcode::Fma:     return IrOpcode::Fma;
    case ast::Opcode::Div:     return IrOpcode::Div;
    case ast::Opcode::Rem:     return IrOpcode::Rem;
    case ast::Opcode::Neg:     return IrOpcode::Neg;
    case ast::Opcode::Abs:     return IrOpcode::Abs;
    case ast::Opcode::Min:     return IrOpcode::Min;
    case ast::Opcode::Max:     return IrOpcode::Max;
    case ast::Opcode::And:     return IrOpcode::And;
    case ast::Opcode::Or:      return IrOpcode::Or;
    case ast::Opcode::Xor:     return IrOpcode::Xor;
    case ast::Opcode::Not:     return IrOpcode::Not;
    case ast::Opcode::Shl:     return IrOpcode::Shl;
    case ast::Opcode::Shr:     return IrOpcode::Shr;
    case ast::Opcode::Cvt:     return IrOpcode::Cvt;
    case ast::Opcode::Cvta:    return IrOpcode::Cvta;
    case ast::Opcode::Setp:    return IrOpcode::Setp;
    case ast::Opcode::Selp:    return IrOpcode::Selp;
    case ast::Opcode::Ld:      return IrOpcode::Ld;
    case ast::Opcode::St:      return IrOpcode::St;
    case ast::Opcode::Bra:     return IrOpcode::Bra; // refined later
    case ast::Opcode::Ret:     return IrOpcode::Ret;
    case ast::Opcode::Exit:    return IrOpcode::Exit;
    case ast::Opcode::BarSync: return IrOpcode::BarSync;
    case ast::Opcode::Atom:    return IrOpcode::Atom;
    default:                   return IrOpcode::Unknown;
    }
}

// ============================================================
// Main entry point
// ============================================================

std::unique_ptr<Function> CFGBuilder::build(const ast::Kernel& kernel,
                                             const std::string& filename) {
    filename_ = filename;
    auto fn = std::make_unique<Function>();
    fn->name    = kernel.name;
    fn->isEntry = kernel.isEntry;
    fn_ = fn.get();

    allocateVRegs(kernel);
    buildBlocks(kernel);
    connectEdges();

    return fn;
}

// ============================================================
// Step 1: Allocate virtual registers
// ============================================================

void CFGBuilder::allocateVRegs(const ast::Kernel& kernel) {
    regMap_.clear();

    // First handle .param registers
    for (const auto& p : kernel.params) {
        regMap_[p.name] = InvalidVReg;
    }

    // Register declarations
    for (const auto& rd : kernel.regDecls) {
        IrType ty = mapType(rd.type);
        if (rd.count > 1) {
            for (int i = 0; i < rd.count; ++i) {
                std::string full = rd.name + std::to_string(i);
                regMap_[full] = fn_->newVReg(ty, full);
            }
        } else {
            regMap_[rd.name] = fn_->newVReg(ty, rd.name);
        }
    }
}

// ============================================================
// Step 2: Build basic blocks from the statement list
// ============================================================

void CFGBuilder::buildBlocks(const ast::Kernel& kernel) {
    // Identify all label names so we know which identifiers are branch targets
    std::unordered_set<std::string> labelNames;
    for (const auto& st : kernel.body) {
        if (st.label) labelNames.insert(st.label->name);
    }

    // We build one "current block" and start a new one whenever:
    //   - We encounter a label definition after at least one instruction
    //   - The previous instruction was a terminator (branch/ret/exit)
    BlockId nextId = 0;

    auto makeBlock = [&](std::string lbl = "") -> BasicBlock* {
        auto blk = std::make_unique<BasicBlock>(nextId++, lbl);
        auto* ptr = blk.get();
        fn_->blocks.push_back(std::move(blk));
        return ptr;
    };

    // Create entry block
    BasicBlock* cur = makeBlock("__entry__");
    // Track program points
    uint32_t pp = 0;
    cur->firstPoint = pp;

    for (const auto& st : kernel.body) {
        // A label starts a new block if the current block has instructions
        // or if the current block already has a terminator
        if (st.label) {
            labelMap_[st.label->name] = nullptr; // placeholder

            // If current block is non-empty (or is the entry block with instructions),
            // start a new block for this label
            if (!cur->insns.empty()) {
                // If cur doesn't end with a terminator, it falls through
                cur->lastPoint = pp > 0 ? pp - 1 : 0;
                BasicBlock* next = makeBlock(st.label->name);
                next->firstPoint = pp;
                cur = next;
            } else {
                // Current block is empty — give it this label
                cur->label = st.label->name;
            }
            labelMap_[st.label->name] = cur;
        }

        // Lower the instruction if present
        if (st.insn) {
            lowerInsn(*st.insn, cur);
            ++pp;

            // If the instruction is a terminator, start a new block
            if (!cur->insns.empty() && cur->insns.back().isTerminator()) {
                cur->lastPoint = pp - 1;
                BasicBlock* next = makeBlock();
                next->firstPoint = pp;
                cur = next;
            }
        }
    }

    // Finalize last block
    cur->lastPoint = pp > 0 ? pp - 1 : 0;

    // Remove any empty trailing blocks
    while (fn_->blocks.size() > 1 && fn_->blocks.back()->insns.empty()) {
        fn_->blocks.pop_back();
    }
}

// ============================================================
// Step 3: Connect edges
// ============================================================

void CFGBuilder::connectEdges() {
    // For each block, examine its terminator to find successors.
    // Fall-through edges: if last insn is NOT a terminator, the next block is the successor.
    for (std::size_t i = 0; i < fn_->blocks.size(); ++i) {
        auto* blk = fn_->blocks[i].get();
        if (blk->insns.empty()) continue;

        auto& last = blk->insns.back();

        if (last.isBranch()) {
            // Explicit branch target
            auto tgt = labelMap_.find(last.branchTarget);
            if (tgt != labelMap_.end() && tgt->second) {
                BlockId tgtId = tgt->second->id;
                blk->succs.push_back(tgtId);
            }

            // Conditional branches also fall through to the next block
            if (last.opcode == IrOpcode::CbrTrue || last.opcode == IrOpcode::CbrFalse) {
                if (i + 1 < fn_->blocks.size()) {
                    blk->succs.push_back(fn_->blocks[i + 1]->id);
                }
            }
        } else if (last.opcode != IrOpcode::Ret && last.opcode != IrOpcode::Exit) {
            // No terminator — fall through
            if (i + 1 < fn_->blocks.size()) {
                blk->succs.push_back(fn_->blocks[i + 1]->id);
            }
        }
    }

    // Build predecessor lists from successor lists
    for (auto& blk : fn_->blocks) {
        for (BlockId s : blk->succs) {
            if (auto* succ = fn_->blockById(s)) {
                succ->preds.push_back(blk->id);
            }
        }
    }
}

// ============================================================
// VReg resolution
// ============================================================

VRegId CFGBuilder::resolveReg(const ast::RegOperand& rop) const {
    std::string full;
    if (rop.index >= 0) {
        full = rop.name + std::to_string(rop.index);
    } else {
        full = rop.name;
    }
    auto it = regMap_.find(full);
    if (it != regMap_.end()) {
        return it->second;
    }
    // Also try base name if rop.index >= 0
    it = regMap_.find(rop.name);
    if (it != regMap_.end() && it->second != InvalidVReg) {
        return it->second + (rop.index >= 0 ? rop.index : 0);
    }
    // Auto-create for special registers or undeclared registers
    IrType ty = IrType::Int32;
    if (full.rfind("%p", 0) == 0) ty = IrType::Pred;
    else if (full.rfind("%rd", 0) == 0) ty = IrType::Int64;
    else if (full.rfind("%f", 0) == 0) ty = IrType::Float32;
    else if (full.rfind("%fd", 0) == 0) ty = IrType::Float64;

    VRegId id = fn_->newVReg(ty, full);
    regMap_[full] = id;
    return id;
}

// ============================================================
// Operand lowering
// ============================================================

ir::Operand CFGBuilder::lowerOperand(const ast::Operand& op, bool /*isDef*/) const {
    if (op.isReg()) {
        VRegId id = resolveReg(op.asReg());
        if (id == InvalidVReg) return ir::Operand::imm("0"); // fallback
        return ir::Operand::reg(id);
    }
    if (op.isImm()) {
        const std::string& raw = op.asImm().raw;
        int64_t v = 0;
        try { v = std::stoll(raw, nullptr, 0); } catch (...) {}
        return ir::Operand::imm(raw, v);
    }
    if (op.isLabel()) {
        return ir::Operand::label(op.asLabel().name);
    }
    if (op.isAddr()) {
        const auto& a = op.asAddr();
        // Check if base is a known register
        auto it = regMap_.find(a.base);
        if (it != regMap_.end() && it->second != InvalidVReg) {
            return ir::Operand::addr(it->second, a.offset.value_or(0));
        }
        // Else treat as global/param symbol
        return ir::Operand::addrGlobal(a.base, a.offset.value_or(0));
    }
    return ir::Operand::imm("0");
}

// ============================================================
// Instruction lowering
// ============================================================

void CFGBuilder::lowerInsn(const ast::Instruction& ai, BasicBlock* block) {
    ir::Instruction ii;
    ii.srcLine = ai.loc.line;
    ii.type    = mapType(ai.mods.type);
    ii.space   = ai.mods.space;
    ii.cmpOp   = ai.mods.cmpOp;
    ii.lo      = ai.mods.lo;
    ii.hi      = ai.mods.hi;
    ii.wide    = ai.mods.wide;

    // Predicate guard
    if (ai.pred) {
        ast::RegOperand rop;
        rop.name = ai.pred->reg;
        rop.index = -1;
        ii.predReg = resolveReg(rop);
        ii.predNeg = ai.pred->negate;
    }

    // Map opcode
    ii.opcode = mapOpcode(ai.opcode);

    // Special cases
    if (ai.opcode == ast::Opcode::Bra) {
        // If there's a predicate guard → conditional branch
        if (ai.pred) {
            ii.opcode = ai.pred->negate ? IrOpcode::CbrFalse : IrOpcode::CbrTrue;
        }
        // Branch target
        if (!ai.srcs.empty() && ai.srcs[0].isLabel()) {
            ii.branchTarget = ai.srcs[0].asLabel().name;
        }
        block->insns.push_back(std::move(ii));
        return;
    }

    // Destinations
    for (const auto& d : ai.dests) {
        ii.defs.push_back(lowerOperand(d, true));
    }

    // Sources
    for (const auto& s : ai.srcs) {
        ii.uses.push_back(lowerOperand(s, false));
    }

    block->insns.push_back(std::move(ii));
}

} // namespace cfg
} // namespace ptxra
