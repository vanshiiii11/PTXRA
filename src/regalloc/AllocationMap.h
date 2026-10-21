#pragma once
#include "PhysReg.h"
#include "../ir/Function.h"
#include <unordered_map>
#include <vector>
#include <string>

namespace ptxra {
namespace regalloc {

/// The result of register allocation.
struct AllocationMap {
    // Physical assignment: vreg → PhysReg
    std::unordered_map<ir::VRegId, PhysReg> assignment;
    // Spill slots: vreg → slot index in local memory
    std::unordered_map<ir::VRegId, int> spillSlot;

    bool hasSpills = false;
    int  scratchSlotsReserved = 0; // e.g. 2 slots reserved when spills occur

    bool isAssigned(ir::VRegId id) const {
        return assignment.count(id) > 0;
    }
    bool isSpilled(ir::VRegId id) const {
        return spillSlot.count(id) > 0;
    }

    PhysReg getPhysReg(ir::VRegId id) const {
        auto it = assignment.find(id);
        if (it != assignment.end()) return it->second;
        return PhysReg::invalid();
    }

    int getSpillSlot(ir::VRegId id) const {
        auto it = spillSlot.find(id);
        if (it != spillSlot.end()) return it->second;
        return -1;
    }

    /// "registers used" = highest slot used + 1, plus nothing else (ptxas semantics).
    /// If scratch slots were reserved for spilling at top of budget, they count as used.
    int registersUsed() const;

    /// Predicates used (highest predicate index + 1, reported separately).
    int predicatesUsed() const;
};

/// Statistics reported after allocation.
struct AllocStats {
    std::string allocatorName;
    int registersUsed   = 0; // highest unified 32-bit slot used + 1
    int predicatesUsed  = 0; // highest predicate used + 1
    int spillStores     = 0;
    int spillLoads      = 0;
    double spillCost    = 0.0; // sum over spill loads/stores of 10^loopDepth
    int scratchSlots    = 0;

    int totalRegsUsed() const { return registersUsed; }
};

} // namespace regalloc
} // namespace ptxra
