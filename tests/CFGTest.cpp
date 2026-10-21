#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "cfg/CFGBuilder.h"
#include "cfg/DOTEmitter.h"
#include <sstream>

using namespace ptxra;

static std::unique_ptr<ir::Function> buildFunction(const std::string& src) {
    Lexer lexer(src);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast::Module mod = parser.parse();
    EXPECT_FALSE(mod.kernels.empty());
    cfg::CFGBuilder builder;
    return builder.build(mod.kernels[0]);
}

TEST(CFGTest, BasicBranchingAndFallthrough) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry test_cfg() {\n"
        "    .reg .pred %p0;\n"
        "    .reg .b32 %r0;\n"
        "    mov.u32 %r0, 1;\n"
        "    setp.eq.s32 %p0, %r0, 0;\n"
        "    @%p0 bra L_target;\n"
        "    mov.u32 %r0, 2;\n"
        "L_target:\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    ASSERT_NE(fn, nullptr);
    ASSERT_GE(fn->blocks.size(), 2u);

    // Entry block should branch to L_target and fall through to the block with mov.u32 %r0, 2
    auto* entry = fn->entry();
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->succs.size(), 2u);

    // Target block has predecessors
    bool foundTarget = false;
    for (const auto& blk : fn->blocks) {
        if (blk->label == "L_target") {
            foundTarget = true;
            EXPECT_GE(blk->preds.size(), 1u);
        }
    }
    EXPECT_TRUE(foundTarget);
}

TEST(CFGTest, MultipleExits) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry test_exits() {\n"
        "    .reg .pred %p0;\n"
        "    setp.eq.s32 %p0, 0, 0;\n"
        "    @%p0 bra L_exit1;\n"
        "    ret;\n"
        "L_exit1:\n"
        "    exit;\n"
        "}\n";

    auto fn = buildFunction(src);
    int exitCount = 0;
    for (const auto& blk : fn->blocks) {
        if (!blk->insns.empty()) {
            auto op = blk->insns.back().opcode;
            if (op == ir::IrOpcode::Ret || op == ir::IrOpcode::Exit) {
                exitCount++;
            }
        }
    }
    EXPECT_GE(exitCount, 2);
}

TEST(CFGTest, DOTEmitterOutput) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".entry test_dot() {\n"
        "    mov.u32 %r0, 1;\n"
        "    ret;\n"
        "}\n";

    auto fn = buildFunction(src);
    cfg::DOTEmitter dot;
    std::ostringstream oss;
    dot.emit(*fn, oss);
    std::string dotStr = oss.str();

    EXPECT_NE(dotStr.find("digraph \"test_dot\""), std::string::npos);
    EXPECT_NE(dotStr.find("node [shape=record]"), std::string::npos);
    EXPECT_NE(dotStr.find("BB0"), std::string::npos);
}
