#include "LoopInfo.h"
#include <queue>
#include <algorithm>

namespace ptxra {
namespace cfg {

// ---------------------------------------------------------------------------
// findLoop — backward BFS from `tail` stopping at `header` to collect
// all basic blocks that belong to the natural loop induced by the
// back edge tail → header.
// ---------------------------------------------------------------------------
NaturalLoop LoopInfo::findLoop(ir::BlockId header, ir::BlockId tail,
                                const ir::Function& fn) {
    NaturalLoop loop;
    loop.header = header;

    // The header is always part of the loop body.
    loop.body.insert(header);

    // If tail == header (self-loop), nothing more to collect.
    if (tail == header) return loop;

    // BFS backward: start from tail, follow preds until we hit the header.
    loop.body.insert(tail);
    std::queue<ir::BlockId> worklist;
    worklist.push(tail);

    while (!worklist.empty()) {
        ir::BlockId cur = worklist.front();
        worklist.pop();

        const ir::BasicBlock* blk = fn.blockById(cur);
        if (!blk) continue;

        for (ir::BlockId predId : blk->preds) {
            // Only enqueue blocks not yet in the body.
            // The header is already inserted — stop there.
            if (loop.body.find(predId) == loop.body.end()) {
                loop.body.insert(predId);
                worklist.push(predId);
            }
        }
    }

    return loop;
}

// ---------------------------------------------------------------------------
// compute — identify back edges, build natural loops, assign loop depths.
//
// A back edge is an edge p → s where s dominates p in the dominator tree.
// We detect these by iterating every CFG edge and checking domination.
// ---------------------------------------------------------------------------
void LoopInfo::compute(ir::Function& fn, const DominatorTree& dom) {
    loops.clear();

    // Scan all CFG edges looking for back edges (succ dominates pred).
    for (const auto& blkPtr : fn.blocks) {
        const ir::BasicBlock* pred = blkPtr.get();
        int predRpo = dom.rpoIndex(pred->id);
        if (predRpo < 0) continue; // unreachable block

        for (ir::BlockId succId : pred->succs) {
            int succRpo = dom.rpoIndex(succId);
            if (succRpo < 0) continue;

            // Back edge: the successor (potential header) dominates the predecessor (tail).
            if (dom.dominates(succRpo, predRpo)) {
                loops.push_back(findLoop(succId, pred->id, fn));
            }
        }
    }

    // Compute loop nesting depth for each block:
    // depth[b] = number of loops whose body contains b.
    // Reset all depths first.
    for (auto& blkPtr : fn.blocks)
        blkPtr->loopDepth = 0;

    for (const NaturalLoop& lp : loops) {
        for (ir::BlockId bid : lp.body) {
            ir::BasicBlock* blk = fn.blockById(bid);
            if (blk) ++blk->loopDepth;
        }
    }

    // Also stamp the NaturalLoop::depth field so callers can use it.
    for (NaturalLoop& lp : loops) {
        // Loop depth = loopDepth of the header block
        const ir::BasicBlock* hdr = fn.blockById(lp.header);
        lp.depth = hdr ? hdr->loopDepth : 0;
    }
}

// ---------------------------------------------------------------------------
// loopDepth — count how many loops contain this block.
// ---------------------------------------------------------------------------
int LoopInfo::loopDepth(ir::BlockId id) const {
    int depth = 0;
    for (const NaturalLoop& lp : loops) {
        if (lp.body.count(id)) ++depth;
    }
    return depth;
}

} // namespace cfg
} // namespace ptxra
