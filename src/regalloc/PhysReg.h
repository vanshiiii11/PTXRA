#pragma once
#include "../ir/VirtualReg.h"
#include <cstdint>
#include <string>

namespace ptxra {
namespace regalloc {

/// A physical register assignment in the ptxas-style register model.
///
/// Model:
/// - Predicates live in a dedicated pool of 7 1-bit registers (%p0..%p6).
/// - All other types share a unified pool of N 32-bit slots (%r0..%rN-1).
/// - 32-bit values (Int32, Float32) occupy 1 slot.
/// - 64-bit values (Int64, Float64) occupy an aligned pair of 2 slots (even slot index).
struct PhysReg {
    ir::RegClass regClass = ir::RegClass::Int32;
    int          slot     = -1; // slot index in unified pool, or predicate index
    int          width    = 1;  // 1 for 32-bit & pred; 2 for 64-bit aligned pair

    bool isValid() const { return slot >= 0; }
    bool isPred()  const { return regClass == ir::RegClass::Pred; }

    bool operator==(const PhysReg& o) const {
        return regClass == o.regClass && slot == o.slot && width == o.width;
    }
    bool operator!=(const PhysReg& o) const { return !(*this == o); }
    bool operator<(const PhysReg& o) const {
        if (regClass != o.regClass)
            return static_cast<int>(regClass) < static_cast<int>(o.regClass);
        return slot < o.slot;
    }

    /// Check if this physical register's slot range overlaps with another.
    bool overlaps(const PhysReg& o) const {
        if (isPred() != o.isPred()) return false;
        return slot < (o.slot + o.width) && o.slot < (slot + width);
    }

    std::string toString() const;

    static PhysReg invalid() { return {ir::RegClass::Int32, -1, 1}; }
};

/// Physical register budget limits.
struct RegLimits {
    int maxRegs         = 255; // total unified 32-bit slots (default unconstrained: 255)
    int maxPreds        = 7;   // dedicated predicate registers (%p0..%p6)
    int scratchReserved = 2;   // scratch slots reserved for spill reloads when spills occur

    static RegLimits withMax(int n) {
        RegLimits lim;
        lim.maxRegs = n;
        lim.maxPreds = 7;
        lim.scratchReserved = 2;
        return lim;
    }
};

} // namespace regalloc
} // namespace ptxra
