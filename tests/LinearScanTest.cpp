#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "cfg/CFGBuilder.h"
#include "analysis/Liveness.h"
#include "analysis/LiveInterval.h"
#include "regalloc/LinearScan.h"
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

TEST(LinearScanTest, NoSpillWhenPlentyOfRegs) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry test_no_spill() {\n"
        "    .reg .b32 %r<5>;\n"
        "    mov.u32 %r0, 1;\n"
        "    mov.u32 %r1, 2;\n"
        "    mov.u32 %r2, 3;\n"
        "    add.s32 %r3, %r0, %r1;\n"
        "    add.s32 %r4, %r3, %r2;\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);
    auto intervals = analysis::LiveIntervals::compute(*fn, live);

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(32);
    regalloc::LinearScanAllocator ls(limits);
    regalloc::AllocStats stats;
    auto alloc = ls.allocate(*fn, intervals, stats);

    EXPECT_EQ(stats.spillStores, 0);
    EXPECT_EQ(stats.spillLoads, 0);
    EXPECT_GT(stats.registersUsed, 0);
    EXPECT_LE(stats.registersUsed, 32);

    auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
    EXPECT_TRUE(vres.passed);
}

TEST(LinearScanTest, SpillsWhenRegLimitTooSmall) {
    // 3 simultaneously live values with max 4 registers (including scratch reservation) -> spills occur
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry test_spill() {\n"
        "    .reg .b32 %r<4>;\n"
        "    mov.u32 %r0, 10;\n"
        "    mov.u32 %r1, 20;\n"
        "    mov.u32 %r2, 30;\n"
        "    add.s32 %r3, %r0, %r1;\n"
        "    add.s32 %r3, %r3, %r2;\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);
    auto intervals = analysis::LiveIntervals::compute(*fn, live);

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(2);
    regalloc::LinearScanAllocator ls(limits);
    regalloc::AllocStats stats;
    auto alloc = ls.allocate(*fn, intervals, stats);

    EXPECT_GT(stats.spillStores, 0);
    EXPECT_LE(stats.registersUsed, 2);

    auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
    EXPECT_TRUE(vres.passed);
}

TEST(LinearScanTest, Aligned64BitPairs) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry test_64bit() {\n"
        "    .reg .b64 %rd<2>;\n"
        "    mov.u64 %rd0, 100;\n"
        "    mov.u64 %rd1, 200;\n"
        "    add.s64 %rd0, %rd0, %rd1;\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);
    auto intervals = analysis::LiveIntervals::compute(*fn, live);

    regalloc::RegLimits limits = regalloc::RegLimits::withMax(16);
    regalloc::LinearScanAllocator ls(limits);
    regalloc::AllocStats stats;
    auto alloc = ls.allocate(*fn, intervals, stats);

    EXPECT_EQ(stats.spillStores, 0);
    for (const auto& [vreg, pr] : alloc.assignment) {
        if (!pr.isPred()) {
            EXPECT_EQ(pr.width, 2);
            EXPECT_EQ(pr.slot % 2, 0); // must be even-aligned
        }
    }

    auto vres = regalloc::AllocationVerifier::verify(*fn, intervals, alloc, limits);
    EXPECT_TRUE(vres.passed);
}
