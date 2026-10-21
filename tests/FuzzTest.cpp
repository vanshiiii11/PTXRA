#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "cfg/CFGBuilder.h"
#include "cfg/Dominator.h"
#include "cfg/LoopInfo.h"
#include "analysis/Liveness.h"
#include "analysis/LiveInterval.h"
#include "regalloc/LinearScan.h"
#include "regalloc/GraphColor.h"
#include "regalloc/Verifier.h"
#include <random>
#include <sstream>

using namespace ptxra;

static std::unique_ptr<ir::Function> buildFunction(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast::Module mod = parser.parse();
    cfg::CFGBuilder builder;
    return builder.build(mod.kernels[0]);
}

static void testBothAllocators(const std::string& src, int maxRegs = 16) {
    auto fn = buildFunction(src);
    cfg::DominatorTree dom;
    dom.compute(*fn);
    cfg::LoopInfo loops;
    loops.compute(*fn, dom);

    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);
    auto intervals = analysis::LiveIntervals::compute(*fn, live);

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(maxRegs);

    // Linear scan
    {
        regalloc::LinearScanAllocator ls(limits);
        regalloc::AllocStats stats;
        auto alloc = ls.allocate(*fn, intervals, stats);
        auto res = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
        EXPECT_TRUE(res.passed) << "Linear scan verifier failed:\n" << (res.errors.empty() ? "" : res.errors[0]);
    }

    // Graph color
    {
        regalloc::GraphColorAllocator gc(limits);
        regalloc::AllocStats stats;
        auto alloc = gc.allocate(*fn, intervals, loops, stats);
        auto res = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
        EXPECT_TRUE(res.passed) << "Graph color verifier failed:\n" << (res.errors.empty() ? "" : res.errors[0]);
    }
}

TEST(FuzzTest, EmptyKernel) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry empty_kernel() {\n"
        "    ret;\n"
        "}\n";
    testBothAllocators(src, 32);
}

TEST(FuzzTest, SingleBlock) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry single_block() {\n"
        "    .reg .b32 %r0;\n"
        "    mov.u32 %r0, 42;\n"
        "    ret;\n"
        "}\n";
    testBothAllocators(src, 32);
}

TEST(FuzzTest, InfiniteLoop) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry infinite_loop() {\n"
        "    .reg .b32 %r0;\n"
        "    mov.u32 %r0, 0;\n"
        "L_loop:\n"
        "    add.s32 %r0, %r0, 1;\n"
        "    bra L_loop;\n"
        "}\n";
    testBothAllocators(src, 32);
}

TEST(FuzzTest, IrreducibleLookingControlFlow) {
    // Cross-jumping branches
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry cross_jumping() {\n"
        "    .reg .pred %p0, %p1;\n"
        "    .reg .b32 %r0, %r1;\n"
        "    mov.u32 %r0, 1;\n"
        "    setp.eq.s32 %p0, %r0, 0;\n"
        "    @%p0 bra L_B;\n"
        "L_A:\n"
        "    add.s32 %r0, %r0, 1;\n"
        "    setp.ge.s32 %p1, %r0, 10;\n"
        "    @%p1 bra L_exit;\n"
        "L_B:\n"
        "    sub.s32 %r0, %r0, 2;\n"
        "    bra L_A;\n"
        "L_exit:\n"
        "    ret;\n"
        "}\n";
    testBothAllocators(src, 16);
}

TEST(FuzzTest, KernelNeedingMoreRegistersThanLimit) {
    // 10 simultaneously live registers with limit of 4
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry tight_limit() {\n"
        "    .reg .b32 %r<10>;\n"
        "    mov.u32 %r0, 1;\n"
        "    mov.u32 %r1, 2;\n"
        "    mov.u32 %r2, 3;\n"
        "    mov.u32 %r3, 4;\n"
        "    mov.u32 %r4, 5;\n"
        "    mov.u32 %r5, 6;\n"
        "    mov.u32 %r6, 7;\n"
        "    mov.u32 %r7, 8;\n"
        "    mov.u32 %r8, 9;\n"
        "    add.s32 %r9, %r0, %r1;\n"
        "    add.s32 %r9, %r9, %r2;\n"
        "    add.s32 %r9, %r9, %r3;\n"
        "    add.s32 %r9, %r9, %r4;\n"
        "    add.s32 %r9, %r9, %r5;\n"
        "    add.s32 %r9, %r9, %r6;\n"
        "    add.s32 %r9, %r9, %r7;\n"
        "    add.s32 %r9, %r9, %r8;\n"
        "    ret;\n"
        "}\n";
    testBothAllocators(src, 4); // max 4 registers
}

TEST(FuzzTest, RandomStraightLineAndBranchingFuzz) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> numVRegsDist(5, 20);
    std::uniform_int_distribution<int> numStepsDist(15, 35);
    std::uniform_int_distribution<int> maxRegsDist(4, 12);

    for (int iter = 0; iter < 10; ++iter) {
        int numVRegs = numVRegsDist(rng);
        int numSteps = numStepsDist(rng);
        int maxRegs = maxRegsDist(rng);

        std::ostringstream ss;
        ss << ".version 7.5\n.target sm_80\n.entry fuzz_" << iter << "() {\n";
        ss << "    .reg .pred %p<2>;\n";
        ss << "    .reg .b32 %r<" << numVRegs << ">;\n";

        // Initialize all vregs
        for (int i = 0; i < numVRegs; ++i) {
            ss << "    mov.u32 %r" << i << ", " << (i * 3 + 1) << ";\n";
        }

        // Generate random operations and optional branches
        std::uniform_int_distribution<int> vregDist(0, numVRegs - 1);
        std::uniform_int_distribution<int> opDist(0, 3);

        for (int s = 0; s < numSteps; ++s) {
            int dst = vregDist(rng);
            int src1 = vregDist(rng);
            int src2 = vregDist(rng);
            int op = opDist(rng);

            if (op == 0) {
                ss << "    add.s32 %r" << dst << ", %r" << src1 << ", %r" << src2 << ";\n";
            } else if (op == 1) {
                ss << "    sub.s32 %r" << dst << ", %r" << src1 << ", %r" << src2 << ";\n";
            } else if (op == 2) {
                ss << "    mul.lo.s32 %r" << dst << ", %r" << src1 << ", %r" << src2 << ";\n";
            } else {
                ss << "    and.b32 %r" << dst << ", %r" << src1 << ", %r" << src2 << ";\n";
            }
        }

        ss << "    ret;\n}\n";

        testBothAllocators(ss.str(), maxRegs);
    }
}
