#pragma once
#include "../ir/Function.h"
#include <unordered_set>
#include <vector>
#include <unordered_map>

namespace ptxra {
namespace analysis {

using LiveSet = std::unordered_set<ir::VRegId>;

/// Per-block liveness sets produced by the backward dataflow pass.
struct BlockLiveness {
    LiveSet def;     ///< Registers defined in this block (first def kills prior live-in)
    LiveSet use;     ///< Upward-exposed uses: used before any def in this block
    LiveSet liveIn;  ///< Live at block entry:  use[B] ∪ (liveOut[B] − def[B])
    LiveSet liveOut; ///< Live at block exit:   ∪ liveIn[S] for S ∈ succs(B)
};

/// Complete liveness result for a function.
struct LivenessResult {
    std::vector<BlockLiveness> blocks; ///< One entry per block, indexed by RPO position
    const ir::Function* fn = nullptr;

    /// Lookup liveness info for a block by its BlockId.
    const BlockLiveness& forBlock(ir::BlockId id) const;

    /// Return the RPO index of a block (throws std::out_of_range on miss).
    int blockIndex(ir::BlockId id) const;
};

/// Computes iterative liveness analysis (backward dataflow).
///
/// Algorithm:
///   1. Compute def/use sets for each block.
///   2. Iterate:
///        liveOut[B] = ∪ liveIn[S]  for S ∈ succs(B)
///        liveIn[B]  = use[B] ∪ (liveOut[B] − def[B])
///   3. Repeat until stable.
///
/// Convergence is guaranteed because the lattice is finite and the transfer
/// function is monotone.  RPO traversal minimises the required iterations
/// (typically 2–3 passes even for loops).
struct LivenessAnalysis {
    LivenessResult compute(const ir::Function& fn);
};

} // namespace analysis
} // namespace ptxra
