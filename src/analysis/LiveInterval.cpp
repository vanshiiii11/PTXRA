#include "LiveInterval.h"
#include <algorithm>
#include <limits>

namespace ptxra {
namespace analysis {

// ---------------------------------------------------------------------------
// LiveIntervals::compute
// ---------------------------------------------------------------------------

LiveIntervals LiveIntervals::compute(const ir::Function& fn,
                                     const LivenessResult& live)
{
    const std::size_t numVRegs = fn.numVRegs();
    const auto rpo = fn.rpo(); // RPO block order (entry first)

    // -----------------------------------------------------------------------
    // Step 1 – Initialise one LiveInterval per vreg with sentinel values.
    //   start = MAX  → will be pulled down when we see the first use/def
    //   end   = 0    → will be pushed up as the interval extends
    // -----------------------------------------------------------------------
    constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();

    LiveIntervals result;
    result.intervals.resize(numVRegs);
    for (std::size_t i = 0; i < numVRegs; ++i) {
        result.intervals[i].vreg  = static_cast<ir::VRegId>(i);
        result.intervals[i].start = kMax;
        result.intervals[i].end   = 0;
    }

    // Helper: extend interval [start,end] to include point p.
    auto extend = [&](ir::VRegId v, uint32_t p) {
        if (v >= numVRegs) return; // guard against stale IDs
        LiveInterval& iv = result.intervals[v];
        iv.start = std::min(iv.start, p);
        iv.end   = std::max(iv.end,   p);
    };

    // -----------------------------------------------------------------------
    // Step 2 – Walk blocks in RPO, assign program points, fill intervals.
    // -----------------------------------------------------------------------
    uint32_t pp = 0; // running program-point counter

    for (ir::BasicBlock* bb : rpo) {
        // Record the first program point of this block.
        // BasicBlock::firstPoint/lastPoint are mutable data members filled here.
        // rpo() returns non-const pointers, so direct write is fine.
        const uint32_t blockFirst = 2u * pp;
        bb->firstPoint = blockFirst;

        for (const ir::Instruction& insn : bb->insns) {
            const uint32_t usePt = 2u * pp;      // "before" instruction: uses
            const uint32_t defPt = 2u * pp + 1u; // "after"  instruction: defs

            // Uses are live at usePt.
            for (ir::VRegId u : insn.useRegs()) {
                extend(u, usePt);
                result.intervals[u].useDefCount++;
            }

            // Defs are first live at defPt (the result isn't live before it).
            for (ir::VRegId d : insn.defRegs()) {
                extend(d, defPt);
                result.intervals[d].useDefCount++;
            }

            ++pp;
        }

        // The last point of this block is the def-side of the final instruction.
        // If the block is empty (shouldn't happen in well-formed IR), keep it
        // at blockFirst so liveOut extension below is a no-op.
        const uint32_t blockLast = (pp > 0) ? (2u * pp - 1u) : blockFirst;
        bb->lastPoint = blockLast;

        // -----------------------------------------------------------------------
        // Step 3 – Extend intervals of liveOut vregs to the end of this block.
        //   A value live out of B must be kept alive through B's last point so
        //   that any successor that uses it (or passes it further) sees it alive.
        // -----------------------------------------------------------------------
        const BlockLiveness& bls = live.forBlock(bb->id);
        for (ir::VRegId v : bls.liveOut) {
            // Also ensure the interval starts no later than blockFirst so that
            // a value entering the block live-in is contiguous from block entry.
            if (v < numVRegs) {
                LiveInterval& iv = result.intervals[v];
                iv.start = std::min(iv.start, blockFirst);
                iv.end   = std::max(iv.end,   blockLast);
            }
        }

        // Similarly, liveIn vregs must be alive at block entry.
        for (ir::VRegId v : bls.liveIn) {
            if (v < numVRegs) {
                LiveInterval& iv = result.intervals[v];
                iv.start = std::min(iv.start, blockFirst);
                // end stays at whatever use/def or liveOut already set
            }
        }
    }

    // Total number of program points (each instruction contributes 2).
    result.numPoints = 2u * pp;

    // Fix up any vreg that was never seen (e.g., declared but unused):
    // set start = 1, end = 0 so start > end indicates inactive.
    for (auto& iv : result.intervals) {
        if (iv.start == kMax) {
            iv.start = 1;
            iv.end   = 0;
        }
    }

    return result;
}

} // namespace analysis
} // namespace ptxra
