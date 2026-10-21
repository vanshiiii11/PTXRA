#pragma once
#include "AllocationMap.h"
#include "PhysReg.h"
#include "../ir/Function.h"
#include "../analysis/LiveInterval.h"
#include <string>
#include <vector>

namespace ptxra {
namespace regalloc {

struct VerificationResult {
    bool passed = true;
    std::vector<std::string> errors;

    void addError(std::string msg) {
        passed = false;
        errors.push_back(std::move(msg));
    }
};

/// Verifies that an allocation satisfies all ptxas-style invariants:
/// 1. Budget constraint: All assigned unified slots and scratch slots satisfy slot + width <= maxRegs.
/// 2. Alignment constraint: 64-bit values occupy even-aligned pairs (slot % 2 == 0, width == 2).
/// 3. Interference constraint: No two overlapping intervals share any slot.
/// 4. Predicate constraint: Predicates in [0, maxPreds).
/// 5. Completeness: Every active vreg is assigned or spilled.
class AllocationVerifier {
public:
    static VerificationResult verify(const ir::Function& fn,
                                     const analysis::LiveIntervals& intervals,
                                     const AllocationMap& alloc,
                                     const RegLimits& limits);
};

} // namespace regalloc
} // namespace ptxra
