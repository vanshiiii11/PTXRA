#include "Dominator.h"
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <stack>

namespace ptxra {
namespace cfg {

// Sentinel for "not yet processed" during the fixed-point iteration.
static constexpr int UNDEFINED = -1;

// ---------------------------------------------------------------------------
void DominatorTree::compute(const ir::Function& fn) {
    rpoOrder = fn.rpo();
    const int n = static_cast<int>(rpoOrder.size());
    if (n == 0) return;

    // Map each block id → its RPO index for fast look-up.
    std::unordered_map<ir::BlockId, int> rpoIdx;
    rpoIdx.reserve(n);
    for (int i = 0; i < n; ++i)
        rpoIdx[rpoOrder[i]->id] = i;

    // Initialize idom: entry dominates itself; all others undefined.
    idom.assign(n, UNDEFINED);
    idom[0] = 0;

    bool changed = true;
    while (changed) {
        changed = false;
        // Iterate in RPO order, skipping the entry block (index 0).
        for (int b = 1; b < n; ++b) {
            const ir::BasicBlock* blk = rpoOrder[b];

            // Pick the first predecessor that has already been processed.
            int newIdom = UNDEFINED;
            for (ir::BlockId predId : blk->preds) {
                auto it = rpoIdx.find(predId);
                if (it == rpoIdx.end()) continue;
                int p = it->second;
                if (idom[p] == UNDEFINED) continue; // not yet processed
                if (newIdom == UNDEFINED) {
                    newIdom = p; // first processed predecessor
                } else {
                    // Intersect to find common dominator of p and current newIdom.
                    newIdom = intersect(p, newIdom);
                }
            }

            if (newIdom != UNDEFINED && idom[b] != newIdom) {
                idom[b] = newIdom;
                changed = true;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// intersect — walk up idom tree from b1 and b2 until they meet.
// This is the finger algorithm from the Cooper et al. paper.
// ---------------------------------------------------------------------------
int DominatorTree::intersect(int b1, int b2) const {
    // Both fingers walk up the dominator tree (toward lower RPO indices)
    // until they meet at the common dominator.
    while (b1 != b2) {
        // Move the deeper finger (higher RPO index) upward.
        while (b1 > b2) b1 = idom[b1];
        while (b2 > b1) b2 = idom[b2];
    }
    return b1;
}

// ---------------------------------------------------------------------------
// dominates — true if the block at aRpo dominates the block at bRpo.
// Walk b's idom chain upward; if we reach a, then a dom b.
// ---------------------------------------------------------------------------
bool DominatorTree::dominates(int aRpo, int bRpo) const {
    if (aRpo < 0 || bRpo < 0) return false;
    if (aRpo == bRpo) return true; // every block dominates itself

    // Walk b's idom chain toward the entry.
    int cur = bRpo;
    while (cur != 0) {           // stop at the entry (idom[0] == 0)
        if (idom[cur] == aRpo) return true;
        if (idom[cur] == cur)   break; // reached entry without finding a
        cur = idom[cur];
    }
    // Check if a is the entry (RPO index 0) — entry dominates everything.
    return (aRpo == 0);
}

} // namespace cfg
} // namespace ptxra
