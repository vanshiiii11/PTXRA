#include "LinearScan.h"
#include <algorithm>
#include <vector>
#include <set>
#include <cassert>

namespace ptxra {
namespace regalloc {

using namespace analysis;
using namespace ir;

AllocationMap LinearScanAllocator::allocate(const Function& fn,
                                             const LiveIntervals& intervals,
                                             AllocStats& stats,
                                             const cfg::LoopInfo* loopInfo) {
    stats.allocatorName = "LinearScan";

    // First pass: try with full budget
    AllocationMap alloc;
    AllocStats trialStats = stats;
    runAllocation(fn, intervals, limits_.maxRegs, alloc, trialStats);

    if (alloc.hasSpills && limits_.scratchReserved > 0 && limits_.maxRegs > limits_.scratchReserved) {
        // Spills occurred: reserve scratch slots and re-run to ensure budget is strictly enforced
        alloc = AllocationMap();
        alloc.hasSpills = true;
        alloc.scratchSlotsReserved = limits_.scratchReserved;
        int usableBudget = limits_.maxRegs - limits_.scratchReserved;
        runAllocation(fn, intervals, usableBudget, alloc, stats);
        alloc.scratchSlotsReserved = limits_.scratchReserved;
    } else {
        stats = trialStats;
    }

    runPredicates(fn, intervals, alloc);

    // Compute exact spill stats and loop-weighted spill cost
    stats.spillStores = 0;
    stats.spillLoads = 0;
    stats.spillCost = 0.0;
    if (alloc.hasSpills) {
        for (const auto& blk : fn.blocks) {
            int depth = blk->loopDepth;
            if (depth == 0 && loopInfo) depth = loopInfo->loopDepth(blk->id);
            double weight = std::pow(10.0, depth);
            for (const auto& insn : blk->insns) {
                for (VRegId u : insn.useRegs()) {
                    if (alloc.isSpilled(u)) {
                        ++stats.spillLoads;
                        stats.spillCost += weight;
                    }
                }
                for (VRegId d : insn.defRegs()) {
                    if (alloc.isSpilled(d)) {
                        ++stats.spillStores;
                        stats.spillCost += weight;
                    }
                }
            }
        }
    }

    stats.registersUsed  = alloc.registersUsed();
    stats.predicatesUsed = alloc.predicatesUsed();
    stats.scratchSlots   = alloc.scratchSlotsReserved;

    return alloc;
}

void LinearScanAllocator::runAllocation(const Function& fn,
                                        const LiveIntervals& intervals,
                                        int usableBudget,
                                        AllocationMap& alloc,
                                        AllocStats& stats) {
    auto& fnMut = const_cast<Function&>(fn);

    // Collect all non-predicate active intervals
    std::vector<LiveInterval> sorted;
    for (const auto& iv : intervals.intervals) {
        if (iv.vreg == InvalidVReg || iv.start > iv.end || iv.useDefCount == 0) continue;
        if (fn.vreg(iv.vreg).regClass != RegClass::Pred) {
            sorted.push_back(iv);
        }
    }

    std::sort(sorted.begin(), sorted.end()); // sort by start point ascending

    struct ActiveItem {
        uint32_t end;
        VRegId   vreg;
        int      slot;
        int      width;
    };
    std::vector<ActiveItem> active;

    for (const auto& iv : sorted) {
        RegClass rc = fn.vreg(iv.vreg).regClass;
        int width = (rc == RegClass::Int64 || rc == RegClass::Float64) ? 2 : 1;

        // 1. Expire old intervals
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [&](const ActiveItem& item) {
                                        return item.end < iv.start;
                                    }),
                     active.end());

        // 2. Mark which slots are occupied
        std::vector<bool> occupied(static_cast<std::size_t>(usableBudget), false);
        for (const auto& item : active) {
            for (int s = 0; s < item.width; ++s) {
                int slotIdx = item.slot + s;
                if (slotIdx >= 0 && slotIdx < usableBudget) {
                    occupied[static_cast<std::size_t>(slotIdx)] = true;
                }
            }
        }

        // 3. Find smallest available slot
        int assignedSlot = -1;
        if (width == 1) {
            for (int s = 0; s < usableBudget; ++s) {
                if (!occupied[static_cast<std::size_t>(s)]) {
                    assignedSlot = s;
                    break;
                }
            }
        } else {
            // width == 2: must be even-aligned pair
            for (int s = 0; s + 1 < usableBudget; s += 2) {
                if (!occupied[static_cast<std::size_t>(s)] &&
                    !occupied[static_cast<std::size_t>(s + 1)]) {
                    assignedSlot = s;
                    break;
                }
            }
        }

        if (assignedSlot >= 0) {
            // Assign slot
            alloc.assignment[iv.vreg] = {rc, assignedSlot, width};
            active.push_back({iv.end, iv.vreg, assignedSlot, width});
        } else {
            // Must spill
            alloc.hasSpills = true;

            // Find active interval with largest end point
            auto worstIt = std::max_element(active.begin(), active.end(),
                                            [](const ActiveItem& a, const ActiveItem& b) {
                                                return a.end < b.end;
                                            });

            if (worstIt != active.end() && worstIt->end > iv.end) {
                // Evict the active interval
                VRegId evictedVReg = worstIt->vreg;
                int evictedSlot = worstIt->slot;
                int evictedWidth = worstIt->width;

                alloc.assignment.erase(evictedVReg);
                alloc.spillSlot[evictedVReg] = fnMut.spillSlotCount++;
                ++stats.spillStores;
                ++stats.spillLoads;

                active.erase(worstIt);

                // If evicted width matches or fits our need, try to take its slot
                if (width == evictedWidth || (width == 1 && evictedWidth == 2)) {
                    alloc.assignment[iv.vreg] = {rc, evictedSlot, width};
                    active.push_back({iv.end, iv.vreg, evictedSlot, width});
                } else {
                    // Otherwise spill new interval as well
                    alloc.spillSlot[iv.vreg] = fnMut.spillSlotCount++;
                    ++stats.spillStores;
                    ++stats.spillLoads;
                }
            } else {
                // Spill the new interval
                alloc.spillSlot[iv.vreg] = fnMut.spillSlotCount++;
                ++stats.spillStores;
                ++stats.spillLoads;
            }
        }
    }
}

void LinearScanAllocator::runPredicates(const Function& fn,
                                        const LiveIntervals& intervals,
                                        AllocationMap& alloc) {
    std::vector<LiveInterval> sorted;
    for (const auto& iv : intervals.intervals) {
        if (iv.vreg == InvalidVReg || iv.start > iv.end || iv.useDefCount == 0) continue;
        if (fn.vreg(iv.vreg).regClass == RegClass::Pred) {
            sorted.push_back(iv);
        }
    }
    std::sort(sorted.begin(), sorted.end());

    struct ActivePred {
        uint32_t end;
        int      slot;
    };
    std::vector<ActivePred> active;

    for (const auto& iv : sorted) {
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [&](const ActivePred& p) { return p.end < iv.start; }),
                     active.end());

        std::vector<bool> used(static_cast<std::size_t>(limits_.maxPreds), false);
        for (const auto& p : active) {
            if (p.slot >= 0 && p.slot < limits_.maxPreds) {
                used[static_cast<std::size_t>(p.slot)] = true;
            }
        }

        int slot = -1;
        for (int s = 0; s < limits_.maxPreds; ++s) {
            if (!used[static_cast<std::size_t>(s)]) {
                slot = s;
                break;
            }
        }

        if (slot >= 0) {
            alloc.assignment[iv.vreg] = {RegClass::Pred, slot, 1};
            active.push_back({iv.end, slot});
        } else {
            // Predicate spill (assigned slot 0 fallback)
            alloc.assignment[iv.vreg] = {RegClass::Pred, 0, 1};
        }
    }
}

} // namespace regalloc
} // namespace ptxra
