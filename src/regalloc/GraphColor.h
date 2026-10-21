#pragma once
#include "AllocationMap.h"
#include "PhysReg.h"
#include "../analysis/LiveInterval.h"
#include "../analysis/Liveness.h"
#include "../cfg/LoopInfo.h"
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace ptxra {
namespace regalloc {

/// Chaitin-Briggs graph coloring register allocator with ptxas-style unified 32-bit slot model.
///
/// Features:
/// - One unified pool of N 32-bit slots shared by int32/f32 (1 slot) and int64/f64 (aligned pair).
/// - Dedicated pool of 7 predicate registers (%p0..%p6).
/// - Interference graph built from precise per-point liveness backward walk.
/// - Loop-depth weighted spill cost: sum(10^loopDepth) / (degree * width).
/// - Reserving scratch slots for spill code when spills occur.
class GraphColorAllocator {
public:
    explicit GraphColorAllocator(RegLimits limits) : limits_(limits) {}

    AllocationMap allocate(const ir::Function& fn,
                           const analysis::LiveIntervals& intervals,
                           const cfg::LoopInfo& loopInfo,
                           AllocStats& stats,
                           const analysis::LivenessResult* liveness = nullptr);

private:
    RegLimits limits_;

    struct Node {
        ir::VRegId vreg = ir::InvalidVReg;
        ir::RegClass regClass = ir::RegClass::Int32;
        int width = 1;
        std::unordered_set<ir::VRegId> adj;
        double spillCost = 0.0;
        int currentDegree = 0;
        bool onStack = false;
        bool spilled = false;
    };

    void runAllocation(const ir::Function& fn,
                       const analysis::LiveIntervals& intervals,
                       const cfg::LoopInfo& loopInfo,
                       const analysis::LivenessResult& liveResult,
                       int usableBudget,
                       AllocationMap& alloc,
                       AllocStats& stats);

    void runPredicates(const ir::Function& fn,
                       const analysis::LiveIntervals& intervals,
                       const analysis::LivenessResult& liveResult,
                       AllocationMap& alloc);
};

} // namespace regalloc
} // namespace ptxra
