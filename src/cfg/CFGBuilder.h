#pragma once
#include "../ast/AST.h"
#include "../ir/Function.h"
#include <memory>
#include <vector>

namespace ptxra {
namespace cfg {

/// Lowers an ast::Kernel to an ir::Function with a complete CFG.
///
/// Algorithm:
///   1. First pass: assign virtual register IDs from .reg declarations.
///   2. Second pass: lower each AST instruction to IR instructions.
///      - Identify leaders (first insn, branch targets, successors of branches).
///      - Build BasicBlocks with fall-through and explicit branch edges.
///   3. Third pass: fill predecessor lists.
class CFGBuilder {
public:
    /// Build the IR function for a single kernel.
    std::unique_ptr<ir::Function> build(const ast::Kernel& kernel,
                                        const std::string& filename = "<input>");

private:
    ir::Function*                fn_     = nullptr;
    std::string                  filename_;

    // Maps PTX register full name (e.g. "%r0", "%p1") to VRegId.
    mutable std::unordered_map<std::string, ir::VRegId> regMap_;

    // Maps label name → BasicBlock*
    std::unordered_map<std::string, ir::BasicBlock*> labelMap_;

    // ---- Internal helpers ----

    /// Allocate virtual registers for all .reg declarations.
    void allocateVRegs(const ast::Kernel& kernel);

    /// Resolve a PTX register operand (e.g., "%r5") to a VRegId.
    ir::VRegId resolveReg(const ast::RegOperand& rop) const;

    /// Lower a single AST instruction to one or more IR instructions
    /// and append them to the given block.
    void lowerInsn(const ast::Instruction& insn, ir::BasicBlock* block);

    /// Lower an AST operand to an IR Operand.
    ir::Operand lowerOperand(const ast::Operand& op, bool isDef = false) const;

    /// Map ast::Opcode → ir::IrOpcode (simple 1-to-1 cases).
    static ir::IrOpcode mapOpcode(ast::Opcode op);

    /// Map ast::PtxType → ir::IrType.
    static ir::IrType mapType(ast::PtxType t);

    /// Build the basic-block structure by identifying leaders.
    /// Returns the ordered list of blocks with their instruction ranges.
    void buildBlocks(const ast::Kernel& kernel);

    /// Connect basic block edges (succs/preds).
    void connectEdges();
};

} // namespace cfg
} // namespace ptxra
