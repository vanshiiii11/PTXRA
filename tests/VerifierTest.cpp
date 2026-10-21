#include <gtest/gtest.h>
#include "regalloc/Verifier.h"
#include "ir/Function.h"

using namespace ptxra;

TEST(VerifierTest, DetectsInterferenceViolation) {
    ir::Function fn;
    ir::VRegId v0 = fn.newVReg(ir::IrType::Int32, "%r0");
    ir::VRegId v1 = fn.newVReg(ir::IrType::Int32, "%r1");

    analysis::LiveIntervals intervals;
    intervals.intervals.resize(2);
    // Overlapping intervals
    intervals.intervals[0] = {v0, 0, 10, 1};
    intervals.intervals[1] = {v1, 5, 15, 1};
    intervals.numPoints = 20;

    regalloc::AllocationMap alloc;
    // Illegally assign same physical slot 0 to both!
    alloc.assignment[v0] = {ir::RegClass::Int32, 0, 1};
    alloc.assignment[v1] = {ir::RegClass::Int32, 0, 1};

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(32);
    auto res = regalloc::AllocationVerifier::verify(fn, intervals, alloc, limits);

    EXPECT_FALSE(res.passed);
    EXPECT_FALSE(res.errors.empty());
}

TEST(VerifierTest, DetectsAlignmentViolation) {
    ir::Function fn;
    ir::VRegId v0 = fn.newVReg(ir::IrType::Int64, "%rd0");

    analysis::LiveIntervals intervals;
    intervals.intervals.resize(1);
    intervals.intervals[0] = {v0, 0, 10, 1};
    intervals.numPoints = 12;

    regalloc::AllocationMap alloc;
    // 64-bit value assigned odd slot 1! Must be even-aligned!
    alloc.assignment[v0] = {ir::RegClass::Int64, 1, 2};

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(32);
    auto res = regalloc::AllocationVerifier::verify(fn, intervals, alloc, limits);

    EXPECT_FALSE(res.passed);
}

TEST(VerifierTest, DetectsBudgetViolation) {
    ir::Function fn;
    ir::VRegId v0 = fn.newVReg(ir::IrType::Int32, "%r0");

    analysis::LiveIntervals intervals;
    intervals.intervals.resize(1);
    intervals.intervals[0] = {v0, 0, 10, 1};
    intervals.numPoints = 12;

    regalloc::AllocationMap alloc;
    // Assign register slot 40 when limit is 32
    alloc.assignment[v0] = {ir::RegClass::Int32, 40, 1};

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(32);
    auto res = regalloc::AllocationVerifier::verify(fn, intervals, alloc, limits);

    EXPECT_FALSE(res.passed);
}

TEST(VerifierTest, DetectsUnassignedActiveRegister) {
    ir::Function fn;
    ir::VRegId v0 = fn.newVReg(ir::IrType::Int32, "%r0");

    analysis::LiveIntervals intervals;
    intervals.intervals.resize(1);
    intervals.intervals[0] = {v0, 0, 10, 1}; // active!
    intervals.numPoints = 12;

    regalloc::AllocationMap alloc;
    // Left unassigned and unspilled

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(32);
    auto res = regalloc::AllocationVerifier::verify(fn, intervals, alloc, limits);

    EXPECT_FALSE(res.passed);
}

TEST(VerifierTest, VerifiesSuccessfulWithinBudget) {
    ir::Function fn;
    ir::VRegId v0 = fn.newVReg(ir::IrType::Int32, "%r0");
    ir::VRegId v1 = fn.newVReg(ir::IrType::Int64, "%rd0");

    analysis::LiveIntervals intervals;
    intervals.intervals.resize(2);
    intervals.intervals[0] = {v0, 0, 10, 1};
    intervals.intervals[1] = {v1, 5, 15, 1};
    intervals.numPoints = 20;

    regalloc::AllocationMap alloc;
    // Disjoint slots: v0 at slot 0 (width 1), v1 at even slot 2 (width 2)
    alloc.assignment[v0] = {ir::RegClass::Int32, 0, 1};
    alloc.assignment[v1] = {ir::RegClass::Int64, 2, 2};

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(32);
    auto res = regalloc::AllocationVerifier::verify(fn, intervals, alloc, limits);

    EXPECT_TRUE(res.passed);
    EXPECT_LE(alloc.registersUsed(), limits.maxRegs);
}
