#pragma once
#include "AllocationMap.h"
#include "PhysReg.h"
#include "../analysis/LiveInterval.h"
#include "../cfg/LoopInfo.h"

namespace ptxra {
namespace regalloc {

/// Linear scan register allocator with ptxas-style unified 32-bit slot model.
///
/// Model:
/// - One unified pool of N 32-bit slots shared by int32/f32 (1 slot) and int64/f64 (aligned pair).
/// - Dedicated pool of 7 predicate registers (%p0..%p6).
/// - Scratch slots reserved for spill code if spills occur.
/// - Registers used = highest slot used + 1.
class LinearScanAllocator {
public:
    explicit LinearScanAllocator(RegLimits limits) : limits_(limits) {}

    AllocationMap allocate(const ir::Function& fn,
                           const analysis::LiveIntervals& intervals,
                           AllocStats& stats,
                           const cfg::LoopInfo* loopInfo = nullptr);

private:
    RegLimits limits_;

    void runAllocation(const ir::Function& fn,
                       const analysis::LiveIntervals& intervals,
                       int usableBudget,
                       AllocationMap& alloc,
                       AllocStats& stats);

    void runPredicates(const ir::Function& fn,
                       const analysis::LiveIntervals& intervals,
                       AllocationMap& alloc);
};

} // namespace regalloc
} // namespace ptxra
