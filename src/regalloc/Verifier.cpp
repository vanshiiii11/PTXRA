#include "Verifier.h"
#include "../analysis/Liveness.h"
#include <sstream>

namespace ptxra {
namespace regalloc {

using namespace ir;
using namespace analysis;

VerificationResult AllocationVerifier::verify(const Function& fn,
                                              const LiveIntervals& intervals,
                                              const AllocationMap& alloc,
                                              const RegLimits& limits) {
    VerificationResult res;

    for (std::size_t i = 0; i < fn.numVRegs(); ++i) {
        VRegId vid = static_cast<VRegId>(i);
        const auto& iv = intervals.forVReg(vid);
        if (iv.start > iv.end || iv.useDefCount == 0) continue; // inactive

        bool assigned = alloc.isAssigned(vid);
        bool spilled  = alloc.isSpilled(vid);

        if (!assigned && !spilled) {
            res.addError("Active vreg v" + std::to_string(vid) + " (" + fn.vreg(vid).name
                         + ") is neither assigned nor spilled");
            continue;
        }

        if (assigned && spilled) {
            res.addError("Active vreg v" + std::to_string(vid) + " (" + fn.vreg(vid).name
                         + ") is both assigned and spilled");
        }

        if (assigned) {
            PhysReg pr = alloc.getPhysReg(vid);
            if (!pr.isValid()) {
                res.addError("Invalid PhysReg for v" + std::to_string(vid));
                continue;
            }

            if (pr.isPred()) {
                if (pr.slot < 0 || pr.slot >= limits.maxPreds) {
                    res.addError("Predicate slot out of range for v" + std::to_string(vid)
                                 + ": " + std::to_string(pr.slot) + ", limit is "
                                 + std::to_string(limits.maxPreds));
                }
            } else {
                // Unified 32-bit slot checks
                RegClass rc = fn.vreg(vid).regClass;
                bool is64 = (rc == RegClass::Int64 || rc == RegClass::Float64);

                if (is64 && (pr.slot % 2 != 0)) {
                    res.addError("64-bit vreg v" + std::to_string(vid)
                                 + " must be even-aligned, got slot " + std::to_string(pr.slot));
                }

                if (pr.slot + pr.width > limits.maxRegs) {
                    res.addError("Budget violation for v" + std::to_string(vid)
                                 + ": slot " + std::to_string(pr.slot) + " + width "
                                 + std::to_string(pr.width) + " > maxRegs ("
                                 + std::to_string(limits.maxRegs) + ")");
                }
            }
        }
    }

    // Check that total registers used does not exceed maxRegs
    int totalSlotsUsed = alloc.registersUsed();
    if (totalSlotsUsed > limits.maxRegs) {
        res.addError("Total used slots (" + std::to_string(totalSlotsUsed)
                     + ") exceeds maxRegs (" + std::to_string(limits.maxRegs) + ")");
    }

    // Interference check: no two simultaneously live virtual registers share overlapping physical slots
    if (!fn.blocks.empty()) {
        analysis::LivenessAnalysis la;
        auto liveResult = la.compute(fn);

        for (const auto& blk : fn.blocks) {
            analysis::LiveSet live = liveResult.forBlock(blk->id).liveOut;

            for (auto it = blk->insns.rbegin(); it != blk->insns.rend(); ++it) {
                const auto& insn = *it;
                const auto defs = insn.defRegs();
                const auto uses = insn.useRegs();

                // 1. Defined register cannot overlap with any register live at this definition point
                for (VRegId d : defs) {
                    if (d == InvalidVReg || !alloc.isAssigned(d)) continue;
                    PhysReg prD = alloc.getPhysReg(d);

                    for (VRegId v : live) {
                        if (v == InvalidVReg || v == d || !alloc.isAssigned(v)) continue;
                        PhysReg prV = alloc.getPhysReg(v);
                        if (prD.overlaps(prV)) {
                            std::ostringstream oss;
                            oss << "Interference violation: simultaneously live v" << d
                                << " (" << fn.vreg(d).name << ") at slot " << prD.slot
                                << " overlaps with v" << v << " (" << fn.vreg(v).name
                                << ") at slot " << prV.slot;
                            res.addError(oss.str());
                        }
                    }
                }

                // 2. Co-defined registers in the same instruction cannot overlap
                for (std::size_t i = 0; i < defs.size(); ++i) {
                    VRegId d1 = defs[i];
                    if (d1 == InvalidVReg || !alloc.isAssigned(d1)) continue;
                    PhysReg pr1 = alloc.getPhysReg(d1);

                    for (std::size_t j = i + 1; j < defs.size(); ++j) {
                        VRegId d2 = defs[j];
                        if (d2 == InvalidVReg || !alloc.isAssigned(d2)) continue;
                        PhysReg pr2 = alloc.getPhysReg(d2);

                        if (pr1.overlaps(pr2)) {
                            std::ostringstream oss;
                            oss << "Interference violation: co-defined v" << d1
                                << " (" << fn.vreg(d1).name << ") at slot " << pr1.slot
                                << " overlaps with v" << d2 << " (" << fn.vreg(d2).name
                                << ") at slot " << pr2.slot;
                            res.addError(oss.str());
                        }
                    }
                }

                for (VRegId d : defs) live.erase(d);
                for (VRegId u : uses) if (u != InvalidVReg) live.insert(u);
            }
        }
    } else {
        // Fallback for synthetic unit tests without blocks: check interval intersections
        const auto& ivList = intervals.intervals;
        for (std::size_t i = 0; i < ivList.size(); ++i) {
            VRegId u = ivList[i].vreg;
            if (u == InvalidVReg || ivList[i].start > ivList[i].end || ivList[i].useDefCount == 0) continue;
            if (!alloc.isAssigned(u)) continue;
            PhysReg prU = alloc.getPhysReg(u);

            for (std::size_t j = i + 1; j < ivList.size(); ++j) {
                VRegId v = ivList[j].vreg;
                if (v == InvalidVReg || ivList[j].start > ivList[j].end || ivList[j].useDefCount == 0) continue;
                if (!alloc.isAssigned(v)) continue;
                PhysReg prV = alloc.getPhysReg(v);

                if (ivList[i].intersects(ivList[j])) {
                    if (prU.overlaps(prV)) {
                        std::ostringstream oss;
                        oss << "Interference violation: v" << u << " (" << fn.vreg(u).name << ") ["
                            << ivList[i].start << ", " << ivList[i].end << "] at slot "
                            << prU.slot << " overlaps with v" << v << " (" << fn.vreg(v).name << ") ["
                            << ivList[j].start << ", " << ivList[j].end << "] at slot "
                            << prV.slot;
                        res.addError(oss.str());
                    }
                }
            }
        }
    }

    return res;
}

} // namespace regalloc
} // namespace ptxra
