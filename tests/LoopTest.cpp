#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "cfg/CFGBuilder.h"
#include "cfg/Dominator.h"
#include "cfg/LoopInfo.h"

using namespace ptxra;

static std::unique_ptr<ir::Function> buildFunction(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast::Module mod = parser.parse();
    cfg::CFGBuilder builder;
    return builder.build(mod.kernels[0]);
}

TEST(LoopTest, SingleNaturalLoop) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry loop1() {\n"
        "    .reg .pred %p0;\n"
        "    .reg .b32 %r0;\n"
        "    mov.u32 %r0, 0;\n"
        "L_head:\n"
        "    setp.ge.s32 %p0, %r0, 10;\n"
        "    @%p0 bra L_exit;\n"
        "    add.s32 %r0, %r0, 1;\n"
        "    bra L_head;\n"
        "L_exit:\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    cfg::DominatorTree dom;
    dom.compute(*fn);
    cfg::LoopInfo loops;
    loops.compute(*fn, dom);

    EXPECT_GE(loops.loops.size(), 1u);

    // Body block should have loopDepth >= 1
    int maxDepth = 0;
    for (const auto& blk : fn->blocks) {
        if (blk->loopDepth > maxDepth) maxDepth = blk->loopDepth;
    }
    EXPECT_GE(maxDepth, 1);
}

TEST(LoopTest, NestedLoops) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry nested() {\n"
        "    .reg .pred %p0, %p1;\n"
        "    .reg .b32 %r0, %r1;\n"
        "    mov.u32 %r0, 0;\n"
        "L_outer:\n"
        "    setp.ge.s32 %p0, %r0, 10;\n"
        "    @%p0 bra L_done;\n"
        "    mov.u32 %r1, 0;\n"
        "L_inner:\n"
        "    setp.ge.s32 %p1, %r1, 5;\n"
        "    @%p1 bra L_outer_inc;\n"
        "    add.s32 %r1, %r1, 1;\n"
        "    bra L_inner;\n"
        "L_outer_inc:\n"
        "    add.s32 %r0, %r0, 1;\n"
        "    bra L_outer;\n"
        "L_done:\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    cfg::DominatorTree dom;
    dom.compute(*fn);
    cfg::LoopInfo loops;
    loops.compute(*fn, dom);

    EXPECT_GE(loops.loops.size(), 2u);

    int maxDepth = 0;
    for (const auto& blk : fn->blocks) {
        if (blk->loopDepth > maxDepth) maxDepth = blk->loopDepth;
    }
    EXPECT_GE(maxDepth, 2);
}
