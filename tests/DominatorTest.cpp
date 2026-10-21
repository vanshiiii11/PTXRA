#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "cfg/CFGBuilder.h"
#include "cfg/Dominator.h"

using namespace ptxra;

static std::unique_ptr<ir::Function> buildFunction(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast::Module mod = parser.parse();
    cfg::CFGBuilder builder;
    return builder.build(mod.kernels[0]);
}

TEST(DominatorTest, DiamondGraph) {
    // Diamond: Entry -> (L_left | L_right) -> L_join -> ret
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry diamond() {\n"
        "    .reg .pred %p0;\n"
        "    .reg .b32 %r0;\n"
        "    setp.eq.s32 %p0, %r0, 0;\n"
        "    @%p0 bra L_left;\n"
        "    mov.u32 %r0, 1;\n" // right
        "    bra L_join;\n"
        "L_left:\n"
        "    mov.u32 %r0, 2;\n"
        "L_join:\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    cfg::DominatorTree dom;
    dom.compute(*fn);

    int entryRpo = 0;
    int joinRpo = -1;
    for (int i = 0; i < (int)dom.rpoOrder.size(); ++i) {
        if (dom.rpoOrder[i]->label == "L_join") {
            joinRpo = i;
            break;
        }
    }

    ASSERT_GE(joinRpo, 0);
    // Entry must dominate join
    EXPECT_TRUE(dom.dominates(entryRpo, joinRpo));
}

TEST(DominatorTest, SimpleLoop) {
    // Loop: Entry -> L_head -> L_body -> L_head, L_head -> L_exit
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry loop_dom() {\n"
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

    int headRpo = -1;
    int exitRpo = -1;
    for (int i = 0; i < (int)dom.rpoOrder.size(); ++i) {
        if (dom.rpoOrder[i]->label == "L_head") headRpo = i;
        if (dom.rpoOrder[i]->label == "L_exit") exitRpo = i;
    }

    ASSERT_GE(headRpo, 0);
    ASSERT_GE(exitRpo, 0);

    // Entry dominates head
    EXPECT_TRUE(dom.dominates(0, headRpo));
    // Head dominates exit
    EXPECT_TRUE(dom.dominates(headRpo, exitRpo));
}
