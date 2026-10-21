#include "Liveness.h"
#include <stdexcept>
#include <string>

namespace ptxra {
namespace analysis {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Build a map from BlockId → RPO index for fast lookup.
static std::unordered_map<ir::BlockId, int>
buildRpoIndexMap(const std::vector<ir::BasicBlock*>& rpo)
{
    std::unordered_map<ir::BlockId, int> m;
    m.reserve(rpo.size());
    for (int i = 0; i < static_cast<int>(rpo.size()); ++i)
        m[rpo[i]->id] = i;
    return m;
}

/// Compute per-block def/use sets from the instruction stream.
/// Scanning forward: an upward-exposed use is one that appears before
/// any def of the same vreg within the block.
static void computeDefUse(
    const std::vector<ir::BasicBlock*>& rpo,
    std::vector<BlockLiveness>& bl)
{
    for (int bi = 0; bi < static_cast<int>(rpo.size()); ++bi) {
        const ir::BasicBlock* bb = rpo[bi];
        BlockLiveness& bls = bl[static_cast<std::size_t>(bi)];

        for (const ir::Instruction& insn : bb->insns) {
            // Process uses first so that a use before a def in the same
            // instruction is counted as upward-exposed (important for φ-free IR).
            for (ir::VRegId u : insn.useRegs()) {
                if (bls.def.count(u) == 0) // not yet defined in this block
                    bls.use.insert(u);
            }
            // Then record defs (kills upward exposure from now on).
            for (ir::VRegId d : insn.defRegs()) {
                bls.def.insert(d);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// LivenessResult helpers
// ---------------------------------------------------------------------------

int LivenessResult::blockIndex(ir::BlockId id) const
{
    const std::vector<ir::BasicBlock*> rpo = fn->rpo();
    for (int i = 0; i < static_cast<int>(rpo.size()); ++i)
        if (rpo[i]->id == id) return i;
    throw std::out_of_range("LivenessResult::blockIndex: unknown BlockId "
                            + std::to_string(id));
}

const BlockLiveness& LivenessResult::forBlock(ir::BlockId id) const
{
    return blocks[static_cast<std::size_t>(blockIndex(id))];
}

// ---------------------------------------------------------------------------
// LivenessAnalysis::compute
// ---------------------------------------------------------------------------

LivenessResult LivenessAnalysis::compute(const ir::Function& fn)
{
    // RPO gives us a good traversal order: the predecessor of a block is
    // visited before it in RPO (except back-edges), which helps the
    // backward dataflow converge faster when we process in reverse RPO.
    const std::vector<ir::BasicBlock*> rpo = fn.rpo();
    const std::size_t n = rpo.size();

    // Build index map for O(1) block → index lookups during edge traversal.
    auto idxMap = buildRpoIndexMap(rpo);

    // Initialise result: empty sets for all blocks.
    LivenessResult result;
    result.fn     = &fn;
    result.blocks.resize(n);

    // Step 1 – compute def/use for every block.
    computeDefUse(rpo, result.blocks);

    // Step 2 – iterative backward dataflow until fixed point.
    // We process blocks in reverse RPO (i.e., from the last block to the
    // first) so that successor liveIn information propagates back faster.
    bool changed = true;
    while (changed) {
        changed = false;

        // Traverse in reverse RPO order.
        for (int bi = static_cast<int>(n) - 1; bi >= 0; --bi) {
            const ir::BasicBlock* bb = rpo[static_cast<std::size_t>(bi)];
            BlockLiveness& bls = result.blocks[static_cast<std::size_t>(bi)];

            // liveOut[B] = ∪ liveIn[S] for S ∈ succs(B)
            LiveSet newOut;
            for (ir::BlockId succId : bb->succs) {
                auto it = idxMap.find(succId);
                if (it == idxMap.end()) continue; // dead successor, skip
                const LiveSet& succIn =
                    result.blocks[static_cast<std::size_t>(it->second)].liveIn;
                newOut.insert(succIn.begin(), succIn.end());
            }

            // liveIn[B] = use[B] ∪ (liveOut[B] − def[B])
            LiveSet newIn = bls.use; // start with upward-exposed uses
            for (ir::VRegId v : newOut) {
                if (bls.def.count(v) == 0) // not killed by a def in this block
                    newIn.insert(v);
            }

            // Detect change (we only grow sets, so size check suffices).
            if (newOut.size() != bls.liveOut.size() ||
                newIn.size()  != bls.liveIn.size())
            {
                changed = true;
            }

            bls.liveOut = std::move(newOut);
            bls.liveIn  = std::move(newIn);
        }
    }

    return result;
}

} // namespace analysis
} // namespace ptxra
