#include "GraphColor.h"
#include <cmath>
#include <algorithm>
#include <cassert>

namespace ptxra {
namespace regalloc {

using namespace ir;
using namespace analysis;

AllocationMap GraphColorAllocator::allocate(const Function& fn,
                                             const LiveIntervals& intervals,
                                             const cfg::LoopInfo& loopInfo,
                                             AllocStats& stats,
                                             const analysis::LivenessResult* liveness) {
    stats.allocatorName = "GraphColoring";

    analysis::LivenessResult computedLive;
    if (!liveness && !fn.blocks.empty()) {
        analysis::LivenessAnalysis la;
        computedLive = la.compute(fn);
        liveness = &computedLive;
    }

    AllocationMap alloc;
    AllocStats trialStats = stats;
    if (liveness) {
        runAllocation(fn, intervals, loopInfo, *liveness, limits_.maxRegs, alloc, trialStats);
    } else {
        analysis::LivenessResult dummy;
        runAllocation(fn, intervals, loopInfo, dummy, limits_.maxRegs, alloc, trialStats);
    }

    if (alloc.hasSpills && limits_.scratchReserved > 0 && limits_.maxRegs > limits_.scratchReserved) {
        alloc = AllocationMap();
        alloc.hasSpills = true;
        alloc.scratchSlotsReserved = limits_.scratchReserved;
        int usableBudget = limits_.maxRegs - limits_.scratchReserved;
        if (liveness) {
            runAllocation(fn, intervals, loopInfo, *liveness, usableBudget, alloc, stats);
        } else {
            analysis::LivenessResult dummy;
            runAllocation(fn, intervals, loopInfo, dummy, usableBudget, alloc, stats);
        }
        alloc.scratchSlotsReserved = limits_.scratchReserved;
    } else {
        stats = trialStats;
    }

    if (liveness) {
        runPredicates(fn, intervals, *liveness, alloc);
    } else {
        analysis::LivenessResult dummy;
        runPredicates(fn, intervals, dummy, alloc);
    }

    // Compute exact spill stats and loop-weighted spill cost
    stats.spillStores = 0;
    stats.spillLoads = 0;
    stats.spillCost = 0.0;
    if (alloc.hasSpills) {
        for (const auto& blk : fn.blocks) {
            int depth = blk->loopDepth;
            if (depth == 0) depth = loopInfo.loopDepth(blk->id);
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

void GraphColorAllocator::runAllocation(const Function& fn,
                                        const LiveIntervals& intervals,
                                        const cfg::LoopInfo& loopInfo,
                                        const analysis::LivenessResult& liveResult,
                                        int usableBudget,
                                        AllocationMap& alloc,
                                        AllocStats& stats) {
    (void)stats;
    auto& fnMut = const_cast<Function&>(fn);

    // Filter active non-predicate registers
    std::vector<VRegId> activeVRegs;
    for (std::size_t i = 0; i < fn.numVRegs(); ++i) {
        VRegId vid = static_cast<VRegId>(i);
        const auto& iv = intervals.forVReg(vid);
        if (iv.start <= iv.end && iv.useDefCount > 0 && fn.vreg(vid).regClass != RegClass::Pred) {
            activeVRegs.push_back(vid);
        }
    }

    if (activeVRegs.empty()) return;

    // 1. Build interference graph from precise per-point liveness
    std::unordered_map<VRegId, Node> nodes;
    for (VRegId vid : activeVRegs) {
        Node n;
        n.vreg = vid;
        n.regClass = fn.vreg(vid).regClass;
        n.width = (n.regClass == RegClass::Int64 || n.regClass == RegClass::Float64) ? 2 : 1;
        nodes[vid] = std::move(n);
    }

    if (!fn.blocks.empty() && liveResult.fn != nullptr) {
        // Precise per-point liveness: for each block, walk instructions backward from live-out,
        // and at each definition add an edge between the defined register and every register
        // live at that point (same register class only, here unified non-predicates).
        for (const auto& blk : fn.blocks) {
            analysis::LiveSet live = liveResult.forBlock(blk->id).liveOut;

            for (auto it = blk->insns.rbegin(); it != blk->insns.rend(); ++it) {
                const auto& insn = *it;
                const auto defs = insn.defRegs();
                const auto uses = insn.useRegs();

                for (VRegId d : defs) {
                    if (d == InvalidVReg || fn.vreg(d).regClass == RegClass::Pred) continue;

                    for (VRegId v : live) {
                        if (v == InvalidVReg || v == d || fn.vreg(v).regClass == RegClass::Pred) continue;
                        if (nodes.count(d) && nodes.count(v)) {
                            nodes[d].adj.insert(v);
                            nodes[v].adj.insert(d);
                        }
                    }
                }

                // Multiple definitions in the same instruction also interfere
                for (std::size_t i = 0; i < defs.size(); ++i) {
                    VRegId d1 = defs[i];
                    if (d1 == InvalidVReg || fn.vreg(d1).regClass == RegClass::Pred) continue;
                    for (std::size_t j = i + 1; j < defs.size(); ++j) {
                        VRegId d2 = defs[j];
                        if (d2 == InvalidVReg || fn.vreg(d2).regClass == RegClass::Pred) continue;
                        if (nodes.count(d1) && nodes.count(d2)) {
                            nodes[d1].adj.insert(d2);
                            nodes[d2].adj.insert(d1);
                        }
                    }
                }

                for (VRegId d : defs) live.erase(d);
                for (VRegId u : uses) if (u != InvalidVReg) live.insert(u);
            }
        }
    } else {
        // Fallback for tests with synthetic intervals and no blocks
        for (std::size_t i = 0; i < activeVRegs.size(); ++i) {
            VRegId u = activeVRegs[i];
            const auto& ivU = intervals.forVReg(u);
            for (std::size_t j = i + 1; j < activeVRegs.size(); ++j) {
                VRegId v = activeVRegs[j];
                const auto& ivV = intervals.forVReg(v);
                if (ivU.intersects(ivV)) {
                    nodes[u].adj.insert(v);
                    nodes[v].adj.insert(u);
                }
            }
        }
    }

    // 2. Precompute reference weights with loop depth
    std::unordered_map<VRegId, double> refWeight;
    for (const auto& blk : fn.blocks) {
        int depth = blk->loopDepth;
        if (depth == 0) depth = loopInfo.loopDepth(blk->id);
        double depthWeight = std::pow(10.0, std::min(depth, 6));

        for (const auto& insn : blk->insns) {
            for (VRegId u : insn.useRegs()) refWeight[u] += depthWeight;
            for (VRegId d : insn.defRegs()) refWeight[d] += depthWeight;
        }
    }

    for (auto& [vid, node] : nodes) {
        int degreeSlots = 0;
        for (VRegId neighbor : node.adj) {
            degreeSlots += nodes[neighbor].width;
        }
        node.currentDegree = degreeSlots;
        double weight = refWeight[vid];
        if (weight <= 0.0) weight = 1.0;
        node.spillCost = weight / std::max(1, node.currentDegree * node.width);
    }

    // 3. Simplify and Optimistic Spill phases
    std::vector<VRegId> selectStack;
    std::size_t remaining = activeVRegs.size();

    while (remaining > 0) {
        VRegId cand = InvalidVReg;

        // Try Simplify: degree < usableBudget
        for (auto& [vid, node] : nodes) {
            if (!node.onStack && node.currentDegree < usableBudget) {
                cand = vid;
                break;
            }
        }

        if (cand == InvalidVReg) {
            // Optimistic spill candidate: lowest spill cost
            double minCost = 1e30;
            for (auto& [vid, node] : nodes) {
                if (!node.onStack && node.spillCost < minCost) {
                    minCost = node.spillCost;
                    cand = vid;
                }
            }
        }

        if (cand == InvalidVReg) {
            for (auto& [vid, node] : nodes) {
                if (!node.onStack) { cand = vid; break; }
            }
        }

        Node& n = nodes[cand];
        n.onStack = true;
        selectStack.push_back(cand);
        --remaining;

        for (VRegId neighbor : n.adj) {
            if (!nodes[neighbor].onStack) {
                nodes[neighbor].currentDegree -= n.width;
            }
        }
    }

    // 4. Select phase
    while (!selectStack.empty()) {
        VRegId vid = selectStack.back();
        selectStack.pop_back();

        const Node& n = nodes[vid];

        // Mark blocked slots from colored neighbors
        std::vector<bool> blocked(static_cast<std::size_t>(usableBudget), false);
        for (VRegId neighbor : n.adj) {
            if (alloc.isAssigned(neighbor)) {
                PhysReg pr = alloc.getPhysReg(neighbor);
                for (int s = 0; s < pr.width; ++s) {
                    int slotIdx = pr.slot + s;
                    if (slotIdx >= 0 && slotIdx < usableBudget) {
                        blocked[static_cast<std::size_t>(slotIdx)] = true;
                    }
                }
            }
        }

        // Find smallest free slot
        int assignedSlot = -1;
        if (n.width == 1) {
            for (int s = 0; s < usableBudget; ++s) {
                if (!blocked[static_cast<std::size_t>(s)]) {
                    assignedSlot = s;
                    break;
                }
            }
        } else {
            // Even-aligned pair for 64-bit
            for (int s = 0; s + 1 < usableBudget; s += 2) {
                if (!blocked[static_cast<std::size_t>(s)] &&
                    !blocked[static_cast<std::size_t>(s + 1)]) {
                    assignedSlot = s;
                    break;
                }
            }
        }

        if (assignedSlot >= 0) {
            alloc.assignment[vid] = {n.regClass, assignedSlot, n.width};
        } else {
            // Actual spill
            alloc.hasSpills = true;
            int slot = fnMut.spillSlotCount++;
            alloc.spillSlot[vid] = slot;
        }
    }
}

void GraphColorAllocator::runPredicates(const Function& fn,
                                        const LiveIntervals& intervals,
                                        const analysis::LivenessResult& liveResult,
                                        AllocationMap& alloc) {
    std::vector<VRegId> predVRegs;
    for (std::size_t i = 0; i < fn.numVRegs(); ++i) {
        VRegId vid = static_cast<VRegId>(i);
        const auto& iv = intervals.forVReg(vid);
        if (iv.start <= iv.end && iv.useDefCount > 0 && fn.vreg(vid).regClass == RegClass::Pred) {
            predVRegs.push_back(vid);
        }
    }

    if (predVRegs.empty()) return;

    std::unordered_map<VRegId, std::unordered_set<VRegId>> predAdj;
    if (!fn.blocks.empty() && liveResult.fn != nullptr) {
        for (const auto& blk : fn.blocks) {
            analysis::LiveSet live = liveResult.forBlock(blk->id).liveOut;

            for (auto it = blk->insns.rbegin(); it != blk->insns.rend(); ++it) {
                const auto& insn = *it;
                const auto defs = insn.defRegs();
                const auto uses = insn.useRegs();

                for (VRegId d : defs) {
                    if (d == InvalidVReg || fn.vreg(d).regClass != RegClass::Pred) continue;
                    for (VRegId v : live) {
                        if (v == InvalidVReg || v == d || fn.vreg(v).regClass != RegClass::Pred) continue;
                        predAdj[d].insert(v);
                        predAdj[v].insert(d);
                    }
                }

                for (std::size_t i = 0; i < defs.size(); ++i) {
                    VRegId d1 = defs[i];
                    if (d1 == InvalidVReg || fn.vreg(d1).regClass != RegClass::Pred) continue;
                    for (std::size_t j = i + 1; j < defs.size(); ++j) {
                        VRegId d2 = defs[j];
                        if (d2 == InvalidVReg || fn.vreg(d2).regClass != RegClass::Pred) continue;
                        predAdj[d1].insert(d2);
                        predAdj[d2].insert(d1);
                    }
                }

                for (VRegId d : defs) live.erase(d);
                for (VRegId u : uses) if (u != InvalidVReg) live.insert(u);
            }
        }
    } else {
        for (std::size_t i = 0; i < predVRegs.size(); ++i) {
            VRegId u = predVRegs[i];
            const auto& ivU = intervals.forVReg(u);
            for (std::size_t j = i + 1; j < predVRegs.size(); ++j) {
                VRegId v = predVRegs[j];
                const auto& ivV = intervals.forVReg(v);
                if (ivU.intersects(ivV)) {
                    predAdj[u].insert(v);
                    predAdj[v].insert(u);
                }
            }
        }
    }

    for (VRegId u : predVRegs) {
        std::vector<bool> taken(static_cast<std::size_t>(limits_.maxPreds), false);
        for (VRegId neighbor : predAdj[u]) {
            if (alloc.isAssigned(neighbor)) {
                int slot = alloc.getPhysReg(neighbor).slot;
                if (slot >= 0 && slot < limits_.maxPreds) {
                    taken[static_cast<std::size_t>(slot)] = true;
                }
            }
        }
        int assigned = -1;
        for (int p = 0; p < limits_.maxPreds; ++p) {
            if (!taken[static_cast<std::size_t>(p)]) {
                assigned = p;
                break;
            }
        }
        if (assigned >= 0) {
            alloc.assignment[u] = {RegClass::Pred, assigned, 1};
        } else {
            alloc.assignment[u] = {RegClass::Pred, 0, 1};
        }
    }
}

} // namespace regalloc
} // namespace ptxra
