#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "cfg/CFGBuilder.h"
#include "analysis/Liveness.h"
#include "analysis/LiveInterval.h"
#include "analysis/Pressure.h"

using namespace ptxra;

static std::unique_ptr<ir::Function> buildFunction(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast::Module mod = parser.parse();
    cfg::CFGBuilder builder;
    return builder.build(mod.kernels[0]);
}

TEST(LivenessTest, StraightLineDefUse) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry straight_line() {\n"
        "    .reg .b32 %r<4>;\n"
        "    mov.u32 %r0, 1;\n"
        "    mov.u32 %r1, 2;\n"
        "    add.s32 %r2, %r0, %r1;\n"
        "    add.s32 %r3, %r2, 10;\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);

    // Entry block has no upward-exposed uses
    const auto& bl = live.forBlock(fn->entry()->id);
    EXPECT_TRUE(bl.use.empty());
    // r0, r1, r2, r3 should be defined
    EXPECT_GE(bl.def.size(), 4u);

    auto intervals = analysis::LiveIntervals::compute(*fn, live);
    // r0 and r1 are both live simultaneously before add.s32 %r2, %r0, %r1
    ir::VRegId r0 = 0, r1 = 1;
    EXPECT_TRUE(intervals.forVReg(r0).intersects(intervals.forVReg(r1)));

    auto press = analysis::PressureResult::compute(*fn, intervals);
    // Peak pressure should be at least 2
    EXPECT_GE(press.peakInt32, 2);
}

TEST(LivenessTest, GoldenBranchJoinLiveness) {
    // r0 defined in entry.
    // If branch taken, r0 used in L_left.
    // If not taken, r0 used in right.
    // Then r1 computed and used after L_join.
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry branch_join() {\n"
        "    .reg .pred %p0;\n"
        "    .reg .b32 %r0, %r1, %r2;\n"
        "    mov.u32 %r0, 42;\n"
        "    setp.eq.s32 %p0, %r0, 0;\n"
        "    @%p0 bra L_left;\n"
        "    mov.u32 %r1, 10;\n"
        "    bra L_join;\n"
        "L_left:\n"
        "    mov.u32 %r1, 20;\n"
        "L_join:\n"
        "    add.s32 %r2, %r0, %r1;\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    analysis::LivenessAnalysis la;
    auto live = la.compute(*fn);

    // Find L_join block
    for (const auto& blk : fn->blocks) {
        if (blk->label == "L_join") {
            const auto& bl = live.forBlock(blk->id);
            // r0 must be liveIn to L_join
            ir::VRegId r0 = ir::InvalidVReg;
            for (std::size_t i = 0; i < fn->numVRegs(); ++i) {
                if (fn->vreg(i).name == "%r0") { r0 = i; break; }
            }
            ASSERT_NE(r0, ir::InvalidVReg);
            EXPECT_TRUE(bl.liveIn.count(r0) > 0);
        }
    }
}
