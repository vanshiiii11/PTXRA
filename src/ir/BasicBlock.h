#pragma once
#include "Instruction.h"
#include <vector>
#include <string>
#include <cstdint>
#include <memory>

namespace ptxra {
namespace ir {

using BlockId = uint32_t;
constexpr BlockId InvalidBlock = ~0u;

/// A basic block in the IR CFG.
/// Instructions are stored in order; the last instruction is always a terminator.
struct BasicBlock {
    BlockId                  id;
    std::string              label;  // optional label (empty if synthesized)
    std::vector<Instruction> insns;  // all instructions including terminator

    // CFG edges (filled by CFGBuilder)
    std::vector<BlockId> succs;
    std::vector<BlockId> preds;

    // Loop information (filled by LoopInfo)
    int loopDepth = 0;

    // Program-point indices (filled by liveness)
    uint32_t firstPoint = 0; // program point of first instruction
    uint32_t lastPoint  = 0; // program point of last instruction

    BasicBlock(BlockId i, std::string lbl)
        : id(i), label(std::move(lbl)) {}

    bool hasTerminator() const {
        return !insns.empty() && insns.back().isTerminator();
    }

    const Instruction* terminator() const {
        if (hasTerminator()) return &insns.back();
        return nullptr;
    }
};

} // namespace ir
} // namespace ptxra
