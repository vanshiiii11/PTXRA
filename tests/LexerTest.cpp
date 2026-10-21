#include <gtest/gtest.h>
#include "lexer/Lexer.h"

using namespace ptxra;

TEST(LexerTest, DirectivesAndIdentifiers) {
    std::string src = ".version 7.5\n.target sm_80\n.address_size 64\n.entry my_kernel";
    Lexer lexer(src);
    auto tokens = lexer.tokenize();

    ASSERT_GE(tokens.size(), 8u);
    EXPECT_EQ(tokens[0].kind, TokenKind::DotVersion);
    EXPECT_EQ(tokens[1].kind, TokenKind::FloatLiteral);
    EXPECT_EQ(tokens[1].text, "7.5");
    EXPECT_EQ(tokens[2].kind, TokenKind::DotTarget);
    EXPECT_EQ(tokens[3].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[3].text, "sm_80");
    EXPECT_EQ(tokens[4].kind, TokenKind::DotAddressSize);
    EXPECT_EQ(tokens[5].kind, TokenKind::IntLiteral);
    EXPECT_EQ(tokens[5].text, "64");
    EXPECT_EQ(tokens[6].kind, TokenKind::DotEntry);
    EXPECT_EQ(tokens[7].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[7].text, "my_kernel");
}

TEST(LexerTest, RegistersAndRanges) {
    std::string src = "%r0 %r<10> %rd5 %p1 %tid.x %ntid.x";
    Lexer lexer(src);
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].kind, TokenKind::Register);
    EXPECT_EQ(tokens[0].text, "%r0");
    EXPECT_EQ(tokens[1].kind, TokenKind::Register);
    EXPECT_EQ(tokens[1].text, "%r<10>");
    EXPECT_EQ(tokens[2].kind, TokenKind::Register);
    EXPECT_EQ(tokens[2].text, "%rd5");
    EXPECT_EQ(tokens[3].kind, TokenKind::Register);
    EXPECT_EQ(tokens[3].text, "%p1");
    EXPECT_EQ(tokens[4].kind, TokenKind::Register);
    EXPECT_EQ(tokens[4].text, "%tid.x");
    EXPECT_EQ(tokens[5].kind, TokenKind::Register);
    EXPECT_EQ(tokens[5].text, "%ntid.x");
}

TEST(LexerTest, CommentsAndWhitespace) {
    std::string src = "// line comment\n/* block\ncomment */ mov.u32 %r0, 42; // trailing";
    Lexer lexer(src);
    auto tokens = lexer.tokenize();

    ASSERT_GE(tokens.size(), 6u);
    EXPECT_EQ(tokens[0].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[0].text, "mov");
    EXPECT_EQ(tokens[1].kind, TokenKind::DotU32);
    EXPECT_EQ(tokens[2].kind, TokenKind::Register);
    EXPECT_EQ(tokens[3].kind, TokenKind::Comma);
    EXPECT_EQ(tokens[4].kind, TokenKind::IntLiteral);
    EXPECT_EQ(tokens[4].text, "42");
    EXPECT_EQ(tokens[5].kind, TokenKind::Semicolon);
}

TEST(LexerTest, PredicatesAndOperators) {
    std::string src = "@%p0 bra L1; @!%p1 ret;";
    Lexer lexer(src);
    auto tokens = lexer.tokenize();

    EXPECT_EQ(tokens[0].kind, TokenKind::At);
    EXPECT_EQ(tokens[1].kind, TokenKind::Register);
    EXPECT_EQ(tokens[1].text, "%p0");
    EXPECT_EQ(tokens[2].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[2].text, "bra");
    EXPECT_EQ(tokens[3].kind, TokenKind::Identifier);
    EXPECT_EQ(tokens[3].text, "L1");
    EXPECT_EQ(tokens[4].kind, TokenKind::Semicolon);

    EXPECT_EQ(tokens[5].kind, TokenKind::At);
    EXPECT_EQ(tokens[6].kind, TokenKind::Bang);
    EXPECT_EQ(tokens[7].kind, TokenKind::Register);
    EXPECT_EQ(tokens[7].text, "%p1");
}
