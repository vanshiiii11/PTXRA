#include "AllocationMap.h"
#include <algorithm>

namespace ptxra {
namespace regalloc {

int AllocationMap::registersUsed() const {
    int maxSlot = -1;
    for (const auto& [vreg, pr] : assignment) {
        (void)vreg;
        if (!pr.isPred() && pr.isValid()) {
            int top = pr.slot + pr.width - 1;
            if (top > maxSlot) maxSlot = top;
        }
    }
    if (hasSpills && scratchSlotsReserved > 0) {
        // If scratch slots were reserved, they sit immediately above or at the budget
        if (maxSlot + scratchSlotsReserved > maxSlot) {
            maxSlot += scratchSlotsReserved;
        }
    }
    return (maxSlot >= 0) ? (maxSlot + 1) : 0;
}

int AllocationMap::predicatesUsed() const {
    int maxPred = -1;
    for (const auto& [vreg, pr] : assignment) {
        (void)vreg;
        if (pr.isPred() && pr.isValid()) {
            if (pr.slot > maxPred) maxPred = pr.slot;
        }
    }
    return (maxPred >= 0) ? (maxPred + 1) : 0;
}

} // namespace regalloc
} // namespace ptxra
