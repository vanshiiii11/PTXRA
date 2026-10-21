#pragma once
#include "AllocationMap.h"
#include "../analysis/LiveInterval.h"
#include <vector>
#include <string>
#include <set>

namespace ptxra {
namespace regalloc {

/// Spill manager: inserts ld.local/st.local spill code into the IR function.
class SpillManager {
public:
    explicit SpillManager(ir::Function& fn) : fn_(fn) {}

    /// Allocate a new spill slot for the given virtual register.
    /// Returns the slot index.
    int allocateSlot(ir::VRegId vreg);

    /// After allocation, rewrite the function's instruction list:
    /// For each spilled vreg, insert st.local before each def and ld.local after each use.
    void rewriteSpills(const AllocationMap& alloc);

    int numSlots() const { return fn_.spillSlotCount; }

private:
    ir::Function& fn_;
};

} // namespace regalloc
} // namespace ptxra
