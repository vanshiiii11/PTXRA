#pragma once
#include "../ir/Function.h"
#include <vector>
#include <unordered_map>

namespace ptxra {
namespace cfg {

/// Dominator tree using Cooper-Harvey-Kennedy algorithm.
/// Uses RPO-numbered blocks for efficiency.
struct DominatorTree {
    /// idom[i] = immediate dominator of block with RPO index i.
    /// idom[0] = 0 (entry dominates itself).
    std::vector<int> idom;

    /// RPO order of blocks
    std::vector<ir::BasicBlock*> rpoOrder;

    /// Compute dominator tree for function fn.
    void compute(const ir::Function& fn);

    /// Returns true if block a dominates block b (a's RPO index dominates b's).
    bool dominates(int aRpo, int bRpo) const;

    /// Get RPO index of a block by ID
    int rpoIndex(ir::BlockId id) const {
        for (int i = 0; i < (int)rpoOrder.size(); ++i)
            if (rpoOrder[i]->id == id) return i;
        return -1;
    }

private:
    int intersect(int b1, int b2) const;
};

} // namespace cfg
} // namespace ptxra
