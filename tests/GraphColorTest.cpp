#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "cfg/CFGBuilder.h"
#include "cfg/Dominator.h"
#include "cfg/LoopInfo.h"
#include "analysis/Liveness.h"
#include "analysis/LiveInterval.h"
#include "regalloc/GraphColor.h"
#include "regalloc/Verifier.h"

using namespace ptxra;

static std::unique_ptr<ir::Function> buildFunction(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast::Module mod = parser.parse();
    cfg::CFGBuilder builder;
    return builder.build(mod.kernels[0]);
}

TEST(GraphColorTest, HandComputedTriangleK3) {
    // 3 mutually interfering variables: r0, r1, r2
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry triangle() {\n"
        "    .reg .b32 %r<4>;\n"
        "    mov.u32 %r0, 1;\n"
        "    mov.u32 %r1, 2;\n"
        "    mov.u32 %r2, 3;\n"
        "    add.s32 %r3, %r0, %r1;\n"
        "    add.s32 %r3, %r3, %r2;\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    cfg::DominatorTree dom;
    dom.compute(*fn);
    cfg::LoopInfo loops;
    loops.compute(*fn, dom);

    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);
    auto intervals = analysis::LiveIntervals::compute(*fn, live);

    // With K=3 registers (scratchReserved=0), all 3 get colored with 0 spills
    {
        regalloc::RegLimits limits = regalloc::RegLimits::withMax(3);
        limits.scratchReserved = 0;
        regalloc::GraphColorAllocator gc(limits);
        regalloc::AllocStats stats;
        auto alloc = gc.allocate(*fn, intervals, loops, stats);

        EXPECT_EQ(stats.spillStores, 0);
        EXPECT_LE(stats.registersUsed, 3);
        auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
        EXPECT_TRUE(vres.passed);
    }

    // With K=2 registers, at least one must spill
    {
        regalloc::RegLimits limits = regalloc::RegLimits::withMax(2);
        limits.scratchReserved = 0;
        regalloc::GraphColorAllocator gc(limits);
        regalloc::AllocStats stats;
        auto alloc = gc.allocate(*fn, intervals, loops, stats);

        EXPECT_GT(stats.spillStores, 0);
        EXPECT_LE(stats.registersUsed, 2);
        auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
        EXPECT_TRUE(vres.passed);
    }
}

TEST(GraphColorTest, LoopWeightHeuristicProtectsLoopVariables) {
    // r0 is used in a loop (high weight).
    // r1 is used only outside the loop (low weight).
    // Under pressure, r1 should be chosen to spill rather than r0.
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry loop_weight() {\n"
        "    .reg .pred %p0;\n"
        "    .reg .b32 %r0, %r1, %r2, %r3;\n"
        "    mov.u32 %r0, 0;\n"
        "    mov.u32 %r1, 100;\n"
        "    mov.u32 %r2, 0;\n"
        "L_loop:\n"
        "    setp.ge.s32 %p0, %r2, 10;\n"
        "    @%p0 bra L_exit;\n"
        "    add.s32 %r0, %r0, 1;\n" // r0 used in loop
        "    add.s32 %r2, %r2, 1;\n" // r2 loop counter
        "    bra L_loop;\n"
        "L_exit:\n"
        "    add.s32 %r3, %r0, %r1;\n" // r1 used here
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    cfg::DominatorTree dom;
    dom.compute(*fn);
    cfg::LoopInfo loops;
    loops.compute(*fn, dom);

    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);
    auto intervals = analysis::LiveIntervals::compute(*fn, live);

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(2);
    limits.scratchReserved = 0;
    regalloc::GraphColorAllocator gc(limits);
    regalloc::AllocStats stats;
    auto alloc = gc.allocate(*fn, intervals, loops, stats);

    auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
    EXPECT_TRUE(vres.passed);

    // r1 should be spilled rather than r0 (because r0 is in the loop)
    ir::VRegId r0 = ir::InvalidVReg;
    ir::VRegId r1 = ir::InvalidVReg;
    for (std::size_t i = 0; i < fn->numVRegs(); ++i) {
        if (fn->vreg(i).name == "%r0") r0 = i;
        if (fn->vreg(i).name == "%r1") r1 = i;
    }
    ASSERT_NE(r0, ir::InvalidVReg);
    ASSERT_NE(r1, ir::InvalidVReg);
    EXPECT_TRUE(alloc.isAssigned(r0));
    EXPECT_TRUE(alloc.isSpilled(r1));
}
