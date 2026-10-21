#pragma once
#include "Liveness.h"
#include <vector>
#include <cstdint>
#include <limits>

namespace ptxra {
namespace analysis {

/// A contiguous live range for one virtual register.
///
/// Program-point numbering (set by LiveIntervals::compute):
///   Each instruction i in RPO order is assigned a base point p = 2*i.
///     - point  2*i   : "before" the instruction (use side)
///     - point  2*i+1 : "after"  the instruction (def side, first live point of result)
///
/// A vreg defined at instruction i is live from 2*i+1 onward.
/// A vreg used  at instruction i is live at (at least) 2*i.
struct LiveInterval {
    ir::VRegId vreg;
    uint32_t   start;       ///< First program point where live
    uint32_t   end;         ///< Last  program point where live (inclusive)
    int        useDefCount = 0; ///< Total number of uses+defs (spill-cost heuristic)

    /// True if this interval overlaps [o.start, o.end].
    bool intersects(const LiveInterval& o) const noexcept {
        return start <= o.end && o.start <= end;
    }

    bool operator<(const LiveInterval& o) const noexcept {
        return start < o.start;
    }
};

/// Collection of live intervals for all virtual registers in a function.
struct LiveIntervals {
    std::vector<LiveInterval> intervals; ///< One per vreg, indexed by VRegId
    uint32_t                  numPoints = 0; ///< Total program points assigned

    const LiveInterval& forVReg(ir::VRegId id) const { return intervals[id]; }
    LiveInterval&       forVReg(ir::VRegId id)       { return intervals[id]; }

    /// Compute live intervals from a liveness result.
    ///
    /// Steps:
    ///   1. Assign program points across blocks in RPO order.
    ///   2. Record firstPoint/lastPoint on each BasicBlock.
    ///   3. For each instruction, extend intervals for uses and defs.
    ///   4. For each block, extend intervals of liveOut vregs to lastPoint.
    static LiveIntervals compute(const ir::Function& fn,
                                  const LivenessResult& live);
};

} // namespace analysis
} // namespace ptxra
