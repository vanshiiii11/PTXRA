#pragma once
#include "Dominator.h"
#include "../ir/Function.h"
#include <unordered_set>
#include <vector>

namespace ptxra {
namespace cfg {

/// A natural loop identified by a back-edge (tail → header).
struct NaturalLoop {
    ir::BlockId                    header = ir::InvalidBlock; ///< Loop header (dominator)
    std::unordered_set<ir::BlockId> body;                     ///< All blocks in the loop
    int                            depth  = 0; ///< Nesting depth (set by LoopInfo::compute)

    bool contains(ir::BlockId id) const { return body.count(id) != 0; }
};

/// Identifies natural loops via back-edge detection over the dominator tree.
/// Requires that CFG pred/succ lists are filled (by CFGBuilder) and that a
/// DominatorTree has already been computed.
struct LoopInfo {
    std::vector<NaturalLoop> loops; ///< All detected natural loops

    /// Compute loops for fn given its dominator tree.
    /// Also stamps BasicBlock::loopDepth on every block.
    void compute(ir::Function& fn, const DominatorTree& dom);

    /// Number of loops whose body contains block id.
    int loopDepth(ir::BlockId id) const;

private:
    /// BFS backward from tail to header to collect all loop body blocks.
    static NaturalLoop findLoop(ir::BlockId header, ir::BlockId tail,
                                const ir::Function& fn);
};

} // namespace cfg
} // namespace ptxra
