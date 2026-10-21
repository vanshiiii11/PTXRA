#include <gtest/gtest.h>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/ASTPrinter.h"
#include <sstream>
#include <fstream>

using namespace ptxra;

static ast::Module parseString(const std::string& src, const std::string& fname = "test.ptx") {
    Lexer lexer(src, fname);
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens), fname);
    return parser.parse();
}

TEST(ParserTest, SimpleKernel) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".address_size 64\n"
        ".visible .entry test_kernel(\n"
        "    .param .u32 N\n"
        ")\n"
        "{\n"
        "    .reg .b32 %r<5>;\n"
        "    ld.param.u32 %r0, [N];\n"
        "    mov.u32 %r1, %tid.x;\n"
        "    add.s32 %r2, %r0, %r1;\n"
        "    ret;\n"
        "}\n";

    ast::Module mod = parseString(src);
    EXPECT_EQ(mod.version, "7.5");
    EXPECT_EQ(mod.target, "sm_80");
    EXPECT_EQ(mod.addressSize, 64);
    ASSERT_EQ(mod.kernels.size(), 1u);

    const auto& k = mod.kernels[0];
    EXPECT_EQ(k.name, "test_kernel");
    EXPECT_TRUE(k.isEntry);
    EXPECT_TRUE(k.isVisible);
    ASSERT_EQ(k.params.size(), 1u);
    EXPECT_EQ(k.params[0].name, "N");
    ASSERT_EQ(k.regDecls.size(), 1u);
    EXPECT_EQ(k.regDecls[0].name, "%r");
    EXPECT_EQ(k.regDecls[0].count, 5);
}

TEST(ParserTest, RoundTrip) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".address_size 64\n"
        "\n"
        ".visible .entry round_trip(\n"
        "    .param .u64 ptr,\n"
        "    .param .u32 val\n"
        ")\n"
        "{\n"
        "    .reg .b32 %r<4>;\n"
        "    .reg .b64 %rd<2>;\n"
        "\n"
        "    ld.param.u32 %r0, [val];\n"
        "    ld.param.u64 %rd0, [ptr];\n"
        "    mov.u32 %r1, 10;\n"
        "    add.s32 %r2, %r0, %r1;\n"
        "    st.global.s32 [%rd0], %r2;\n"
        "    ret;\n"
        "}\n\n";

    ast::Module mod1 = parseString(src);

    std::ostringstream oss;
    ast::ASTPrinter printer(oss);
    printer.print(mod1);
    std::string printed = oss.str();

    ast::Module mod2 = parseString(printed);

    // Verify properties match
    EXPECT_EQ(mod1.version, mod2.version);
    EXPECT_EQ(mod1.target, mod2.target);
    EXPECT_EQ(mod1.addressSize, mod2.addressSize);
    ASSERT_EQ(mod1.kernels.size(), mod2.kernels.size());

    const auto& k1 = mod1.kernels[0];
    const auto& k2 = mod2.kernels[0];
    EXPECT_EQ(k1.name, k2.name);
    EXPECT_EQ(k1.params.size(), k2.params.size());
    EXPECT_EQ(k1.regDecls.size(), k2.regDecls.size());
    EXPECT_EQ(k1.body.size(), k2.body.size());
}

TEST(ParserTest, UnsupportedOpcodeThrows) {
    std::string src =
        ".version 7.5\n"
        ".target sm_80\n"
        ".visible .entry bad_kernel() {\n"
        "    nonexistent_op.u32 %r0, %r1;\n"
        "}\n";

    EXPECT_THROW({
        parseString(src);
    }, UnsupportedFeature);
}

TEST(ParserTest, AllSamplesParse) {
    std::vector<std::string> sampleFiles = {
        "samples/vector_add.ptx",
        "samples/saxpy.ptx",
        "samples/reduction.ptx",
        "samples/matmul.ptx",
        "samples/heavy_pressure.ptx",
        "samples/divergent.ptx",
        "samples/stencil.ptx"
    };

    for (const auto& path : sampleFiles) {
        std::ifstream file(path);
        if (!file.is_open()) {
            file.open("../" + path);
        }
        ASSERT_TRUE(file.is_open()) << "Could not open " << path;
        std::stringstream ss;
        ss << file.rdbuf();

        ast::Module mod;
        EXPECT_NO_THROW({
            mod = parseString(ss.str(), path);
        }) << "Failed to parse " << path;

        EXPECT_FALSE(mod.kernels.empty()) << "No kernels in " << path;
    }
}
